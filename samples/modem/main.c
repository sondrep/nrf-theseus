#include <FreeRTOS.h>
#include <task.h>
#include <assert.h>
#include <theseus/log.h>
#include <nrf_modem.h>
#include <string.h>
#include <nrf_socket.h>
#include <nrf_modem_at.h>
#include <theseus/modem.h>
#include <errno.h>
#include <event_groups.h>

#define HOST "duckduckgo.com"
#define PORT "80"
#define HTTP_HEAD                                                                                  \
	"HEAD / HTTP/1.1\r\n"                                                                      \
	"Host: " HOST ":" PORT "\r\n"                                                              \
	"Connection: close\r\n\r\n"
#define HTTP_HEAD_LEN (sizeof(HTTP_HEAD) - 1)

#define HTTP_HEAD_KEEPALIVE                                                                        \
	"HEAD / HTTP/1.1\r\n"                                                                      \
	"Host: " HOST ":" PORT "\r\n"                                                              \
	"Connection: keep-alive\r\n\r\n"
#define HTTP_HEAD_KEEPALIVE_LEN (sizeof(HTTP_HEAD_KEEPALIVE) - 1)

#define NOTIF_HANDLER_BIT (1 << 0)

static int tcp_sock = -1;
static char buf[512];

static SemaphoreHandle_t cereg_sem;

static void notif_handler(const char *notif)
{
	/* Registered, home network; registered: roaming. */
	if (!strncmp(notif, "+CEREG: 1", strlen("+CEREG: 1")) ||
	    !strncmp(notif, "+CEREG: 5", strlen("+CEREG: 5"))) {
		xSemaphoreGive(cereg_sem);
		LOG("Semaphore given\n");
	}
}

static void test_before(void)
{
	int err;
	struct nrf_addrinfo *ai = NULL;
	struct nrf_addrinfo hints = {
		.ai_family = NRF_AF_INET,
		.ai_socktype = NRF_SOCK_STREAM,
		.ai_protocol = NRF_IPPROTO_TCP,
	};

	// mem_check_start();

	err = nrf_getaddrinfo(HOST, PORT, &hints, &ai);
	if (err != 0) {
		LOG("getaddrinfo failed, err %d\n", err);
	}

	tcp_sock = nrf_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (tcp_sock < 0) {
		LOG("nrf_socket failed, tcp_sock = %d\n", tcp_sock);
	}

	err = nrf_connect(tcp_sock, ai->ai_addr, ai->ai_addrlen);
	if (err) {
		LOG("connect() failed, err %d\n", err);
	}

	nrf_freeaddrinfo(ai);

	LOG("Connected\n");
}

static void test(void)
{
	int ret;
	char *p;
	size_t off;
	ssize_t bytes;
	struct nrf_pollfd pollfd[1] = {{
		.events = NRF_POLLIN,
		.fd = tcp_sock,
	}};

	memcpy(buf, HTTP_HEAD, HTTP_HEAD_LEN);
	int size = snprintf(
		buf, sizeof(buf),
		"HEAD %s HTTP/1.1\r\n"		 // Request line
		"Host: %s\r\n"			 // Host header (required)
		"User-Agent: MyCProgram/1.0\r\n" // Identify client (optional but recommended)
		"\r\n",				 // End of headers (double CRLF)
		"/", HOST);

	off = 0;
	do {
		bytes = nrf_send(tcp_sock, buf + off, size, 0);
		if (bytes < 0) {
			LOG("send() failed, bytes = %ld\n", bytes);
		}
		off += bytes;
	} while (off < size);

	LOG("Sent %ld bytes\n", off);

	memset(buf, 0x00, sizeof(buf));
	off = 0;

	ssize_t bytes_read;
	while ((bytes_read = nrf_recv(tcp_sock, buf, sizeof(buf) - 1, 0)) > 0) {
		buf[bytes_read] = '\0';
		LOG("%s", buf);
	}
	if (bytes_read == -1) {
		LOG("nrf_recv error\n");
	}

	LOG("Received %ld bytes\n", off);

	/* Print HTTP response */
	p = strstr(buf, "\r\n");
	if (p) {
		off = p - buf;
		buf[off + 1] = '\0';
		LOG("\n>\t %s\n\n", buf);
	}

	ret = nrf_poll(pollfd, 1, 0);
	if (ret < 1) {
		LOG("poll() failed ret = %d, err = %d\n", ret, errno);
	}
	if (pollfd[0].revents != (NRF_POLLIN | NRF_POLLHUP)) {
		LOG("Unexpected revents 0x%x\n", pollfd[0].revents);
	}

	ret = nrf_close(tcp_sock);
	if (ret) {
		LOG("close() failed, ret = %d\n", ret);
	}
}

static void http_task(void *param)
{
	int err;

	err = modem_init();
	if (err != 0) {
		LOG("[APP] nrf_modem_init_failed error: %d\n", err);
	}

	cereg_sem = xSemaphoreCreateBinary();

	err = nrf_modem_at_notif_handler_set(notif_handler);
	if (err != 0) {
		LOG("[APP] nrf_modem_at_notif_handler_set error: %d\n", err);
	}

	err = nrf_modem_at_printf("AT+CEREG=1");
	if (err != 0) {
		LOG("[APP] nrf_modem_at_printf error: %d\n", err);
	}

	err = nrf_modem_at_printf("AT+CFUN=1");
	if (err != 0) {
		LOG("[APP] nrf_modem_at_printf error: %d\n", err);
	}
	xSemaphoreTake(cereg_sem, portMAX_DELAY);
	LOG("Semaphore TAKEN\n");

	test_before();
	test();
}

int main(void)
{
	int ret = 0;

	LOG("This is a joe mama sample\n");

	BaseType_t ok = xTaskCreate(http_task, "http", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
	assert(ok == pdPASS);

	LOG("[APP] starting scheduler\n");
	vTaskStartScheduler();

	while (1) {
		/* Should never reach here unless the scheduler can't start (eks: no heap). */
	}

	return 0;
}
