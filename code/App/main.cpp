#define FUSE_USE_VERSION 29

#include <stdio.h>
#include <iostream>

#include "../flag.h"
#if EMULATING
#   include "../tests/SGX_Emulator/Enclave_u.hpp"
#else
#   include "Enclave_u.h"
#endif

#include "sgx_utils/sgx_utils.h"
#include "utils/headers/fuse.hpp"
#include "utils/headers/options.hpp"

sgx_enclave_id_t ENCLAVE_ID;
string BINARY_PATH, LAUXUS_DIR;
string CONTENT_DIR, META_DIR, AUDIT_DIR, QUOTE_DIR;
string RK_PATH, ARK_PATH, SUPERNODE_PATH;
static struct fuse_operations lauxus_oper;


#if EMULATING
#else
void setup_fuse() {
  lauxus_oper.init = fuse_init;
  lauxus_oper.destroy = fuse_destroy;

  lauxus_oper.getattr = fuse_getattr;
  lauxus_oper.fgetattr = fuse_fgetattr;
  lauxus_oper.rename = fuse_rename;

  lauxus_oper.open = fuse_open;
  lauxus_oper.release = fuse_release;
  lauxus_oper.create = fuse_create;
  lauxus_oper.read = fuse_read;
  lauxus_oper.write = fuse_write;
  lauxus_oper.truncate = fuse_truncate;
  lauxus_oper.unlink = fuse_unlink;

  lauxus_oper.readdir = fuse_readdir;
  lauxus_oper.mkdir = fuse_mkdir;
  lauxus_oper.rmdir = fuse_rmdir;
}

int main(int argc, char **argv) {
  setup_fuse();
  BINARY_PATH = get_directory_path(argv[0]);
  LAUXUS_DIR = BINARY_PATH + "/.lauxus";
  RK_PATH = BINARY_PATH + "/sealed_rk";
  ARK_PATH = BINARY_PATH + "/sealed_ark";
  META_DIR = LAUXUS_DIR + "/metadata";
  SUPERNODE_PATH = META_DIR + "/0000-00-00-00-000000";
  CONTENT_DIR = LAUXUS_DIR + "/content";
  AUDIT_DIR = LAUXUS_DIR + "/audit";
  QUOTE_DIR = LAUXUS_DIR + "/quotes";

  struct lauxus_options *options = (struct lauxus_options*) malloc(sizeof(struct lauxus_options));
  int main_mount = 0;
  struct fuse_args args = parse_args(argc, argv, options, &main_mount);
  if (main_mount != 1)
    return main_mount < 0 ? -1 : 0;

  int ret = fuse_main(args.argc, args.argv, &lauxus_oper, (void*)options);
  fuse_opt_free_args(&args);
  return ret;
}
#endif
