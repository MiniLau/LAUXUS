#ifndef _SGX_ERROR_H_
#define _SGX_ERROR_H_

typedef enum _status_t {
  SGX_SUCCESS = 0,
  SGX_ERROR_MAC_MISMATCH = -1
} sgx_status_t;

#endif
