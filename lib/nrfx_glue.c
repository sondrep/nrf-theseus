#include <errno.h>

#define STRINGIFY(x) #x

volatile unsigned int theseus_critical_nesting;

const char *nrfx_error_string_get(int code)
{
	switch (-code) {
	case 0: return STRINGIFY(0);
	case ECANCELED: return STRINGIFY(ECANCELED);
	case ENOMEM: return STRINGIFY(ENOMEM);
	case ENOTSUP: return STRINGIFY(ENOTSUP);
	case EINVAL: return STRINGIFY(EINVAL);
	case EINPROGRESS: return STRINGIFY(EINPROGRESS);
	case E2BIG: return STRINGIFY(E2BIG);
	case ETIMEDOUT: return STRINGIFY(ETIMEDOUT);
	case EPERM: return STRINGIFY(EPERM);
	case EFAULT: return STRINGIFY(EFAULT);
	case EACCES: return STRINGIFY(EACCES);
	case EBUSY: return STRINGIFY(EBUSY);
	case EALREADY: return STRINGIFY(EALREADY);
	default: return "unknown";
	}
}
