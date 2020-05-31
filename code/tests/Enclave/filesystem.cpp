#include "../catch.hpp"
#include "../../Enclave/utils/headers/user.hpp"
#include "../../App/utils/headers/serialisation.hpp"
// #include "../../Enclave/utils/headers/filesystem.hpp"
#include "../../Enclave/utils/headers/nodes/node.hpp"
#include "../../Enclave/utils/headers/nodes/supernode.hpp"

#include <cerrno>
#include <string>
#include <cstring>
#include <vector>
#include <assert.h>


using namespace std;
static string CONTENT_DIR = "/tmp/nexus_tests/contents";
static string META_DIR = "/tmp/nexus_tests/metas";
static string AUDIT_DIR = "/tmp/nexus_tests/audits";


// FileSystem* _create_fs(User *root=NULL, User *lambda=NULL, size_t block_size=DEFAULT_BLOCK_SIZE) {
//   lauxus_gcm_t *root_key = (lauxus_gcm_t*) malloc(sizeof(lauxus_gcm_t)); lauxus_random_gcm(root_key);
//   lauxus_gcm_t *audit_root_key = (lauxus_gcm_t*) malloc(sizeof(lauxus_gcm_t)); lauxus_random_gcm(audit_root_key);
//   Supernode *supernode = new Supernode(root_key);
//   if (root != NULL)
//     REQUIRE( supernode->add_user(root) == root );
//   if (lambda != NULL)
//     REQUIRE( supernode->add_user(lambda) == lambda );
//
//   string dir = "/tmp/nexus_tests";
//   create_directory(dir);
//   create_directory(CONTENT_DIR);
//   create_directory(META_DIR);
//   create_directory(AUDIT_DIR);
//
//   assert(read_directory(CONTENT_DIR).size() == 0);
//   assert(read_directory(META_DIR).size() == 0 || read_directory(META_DIR).size() == 1);
//   assert(read_directory(AUDIT_DIR).size() == 0 || read_directory(AUDIT_DIR).size() == 1);
//
//   FileSystem *fs = new FileSystem(root_key, audit_root_key, supernode, CONTENT_DIR, META_DIR, AUDIT_DIR, block_size);
//   fs->current_user = root;
//   fs->e_write_meta_to_disk(fs->supernode);
//
//   return fs;
// }

// User* _create_user() {
//   size_t pk_size = sizeof(sgx_ec256_public_t), sk_size = sizeof(sgx_ec256_private_t);
//   sgx_ec256_public_t pk[pk_size];
//   sgx_ec256_private_t sk[sk_size];
//
//   REQUIRE( User::generate_keys(pk_size, pk, sk_size, sk) == 0 );
//   return new User("test", pk_size, pk);
// }

//
// TEST_CASE( "1: Newly created filesystem, everything must return -ENOENT", "[multi-file:filesystem]" ) {
//   FileSystem *fs = _create_fs();
//
//   REQUIRE( fs->edit_user_entitlement("/test", Node::OWNER_RIGHT, "") == -ENOENT );
//   REQUIRE( fs->readdir("/").size() == 0 );
//   REQUIRE( fs->get_rights("/test") == -ENOENT );
//   REQUIRE( fs->entry_type("/test") == -ENOENT );
//   REQUIRE( fs->file_size("/test") == -ENOENT );
//   REQUIRE( fs->read_file("Testing purpose", "/test", 0, 10, NULL) == -ENOENT );
//   REQUIRE( fs->write_file("Testing purpose", "/test", 0, 4, "Test") == -ENOENT );
//   REQUIRE( fs->unlink("Testing purpose", "/test") == -ENOENT );
//
//   REQUIRE( read_directory(CONTENT_DIR).size() == 0 );
//   REQUIRE( read_directory(META_DIR).size() == 1 );
//
//   delete fs;
// }
//
// TEST_CASE( "2: Filesystem can create and delete file", "[multi-file:filesystem]" ) {
//   User *user = _create_user();
//   FileSystem *fs = _create_fs(user);
//
//   REQUIRE( fs->create_file("Testing purpose", "/test") == 0 );
//   REQUIRE( read_directory(CONTENT_DIR).size() == 0 );
//   REQUIRE( read_directory(META_DIR).size() == 2 );
//   REQUIRE( read_directory(AUDIT_DIR).size() == 2 );
//
//   REQUIRE( fs->unlink("Testing purpose", "/test") == 0 );
//   REQUIRE( read_directory(CONTENT_DIR).size() == 0 );
//   REQUIRE( read_directory(META_DIR).size() == 1 );
//   REQUIRE( read_directory(AUDIT_DIR).size() == 1 );
//
//   delete fs;
// }
//
// TEST_CASE( "3.a: Filesystem can write and read file", "[multi-file:filesystem]" ) {
//   for (int block_size = 10; block_size < 20; block_size+=2) {
//     User *user = _create_user();
//     FileSystem *fs = _create_fs(user, NULL, block_size);
//
//     REQUIRE( fs->create_file("Testing purpose", "/test") == 0 );
//     REQUIRE( fs->file_size("/test") == 0 );
//     REQUIRE( fs->read_file("Testing purpose", "/test", 0, 0, NULL) == 0 );
//     REQUIRE( fs->read_file("Testing purpose", "/test", 0, 16, NULL) == 0 );
//
//     REQUIRE( fs->write_file("Testing purpose", "/test", 0, 16, "This is a test !") == 16 );
//     REQUIRE( fs->file_size("/test") == 16 );
//     REQUIRE( fs->write_file("Testing purpose", "/test", 10, 20, "more advanced test !") == 20 );
//     REQUIRE( fs->file_size("/test") == 30 );
//     REQUIRE( read_directory(CONTENT_DIR).size() == 1 );
//     REQUIRE( read_directory(META_DIR).size() == 2 );
//     REQUIRE( read_directory(AUDIT_DIR).size() == 2 );
//
//     char buffer[30];
//     REQUIRE( fs->read_file("Testing purpose", "/test", 0, 30, buffer) == 30 );
//     REQUIRE( memcmp(buffer, "This is a more advanced test !", 30) == 0 );
//     REQUIRE( fs->read_file("Testing purpose", "/test", 0, 40, buffer) == 30 );
//     REQUIRE( memcmp(buffer, "This is a more advanced test !", 30) == 0 );
//     REQUIRE( fs->read_file("Testing purpose", "/test", 10, 20, buffer) == 20 );
//     REQUIRE( memcmp(buffer, "more advanced test !", 20) == 0 );
//     REQUIRE( fs->read_file("Testing purpose", "/test", 10, 40, buffer) == 20 );
//     REQUIRE( memcmp(buffer, "more advanced test !", 20) == 0 );
//
//     REQUIRE( fs->unlink("Testing purpose", "/test") == 0 );
//     REQUIRE( read_directory(CONTENT_DIR).size() == 0 );
//     REQUIRE( read_directory(META_DIR).size() == 1 );
//     REQUIRE( read_directory(AUDIT_DIR).size() == 1 );
//
//     delete fs;
//   }
// }
//
// TEST_CASE( "3.b: Filesystem creating big file chunk by chunk", "[multi-file:filesystem]" ) {
//   User *user = _create_user();
//   FileSystem *fs = _create_fs(user, NULL, 54);
//
//   string chunk = "This is a simple chunk that will be copied many times.";
//
//   REQUIRE( fs->create_file("Testing purpose", "/test") == 0 );
//   REQUIRE( fs->write_file("Testing purpose", "/test", 0, 54, (char*)chunk.data()) == 54 );
//   REQUIRE( fs->write_file("Testing purpose", "/test", 54, 54, (char*)chunk.data()) == 54 );
//
//   REQUIRE( fs->unlink("Testing purpose", "/test") == 0 );
//   REQUIRE( read_directory(CONTENT_DIR).size() == 0 );
//   REQUIRE( read_directory(META_DIR).size() == 1 );
//   REQUIRE( read_directory(AUDIT_DIR).size() == 1 );
//
//   delete fs;
// }
//
// TEST_CASE( "4: Filesystem can list files in a directory", "[multi-file:filesystem]" ) {
//   User *user = _create_user();
//   FileSystem *fs = _create_fs(user);
//
//   REQUIRE( fs->create_file("Testing purpose", "/test1") == 0 );
//   REQUIRE( fs->create_file("Testing purpose", "/test2") == 0 );
//   REQUIRE( read_directory(CONTENT_DIR).size() == 0 );
//   REQUIRE( read_directory(META_DIR).size() == 3 );
//   REQUIRE( read_directory(AUDIT_DIR).size() == 3 );
//
//   REQUIRE( fs->entry_type("/") == EISDIR );
//   REQUIRE( fs->entry_type("/test1") == EEXIST );
//   REQUIRE( fs->entry_type("/test2") == EEXIST );
//
//   REQUIRE( fs->file_size("/test1") == 0 );
//   REQUIRE( fs->file_size("/test2") == 0 );
//
//   vector<string> ls = fs->readdir("/");
//   REQUIRE( ls.size() == 2 );
//   REQUIRE( find(ls.begin(), ls.end(), "test1") != ls.end() );
//   REQUIRE( find(ls.begin(), ls.end(), "test2") != ls.end() );
//
//   REQUIRE( fs->unlink("Testing purpose", "/test1") == 0 );
//   REQUIRE( fs->unlink("Testing purpose", "/test2") == 0 );
//   REQUIRE( read_directory(CONTENT_DIR).size() == 0 );
//   REQUIRE( read_directory(META_DIR).size() == 1 );
//   REQUIRE( read_directory(AUDIT_DIR).size() == 1 );
//
//   delete fs;
// }
//
// TEST_CASE( "5: Filesystem can allow specific user entitlements", "[multi-file:filesystem]" ) {
//   User *root = _create_user();
//   User *lambda = _create_user();
//   FileSystem *fs = _create_fs(root, lambda);
//
//   REQUIRE( fs->create_file("Testing purpose", "/test1") == 0 );
//   REQUIRE( fs->create_file("Testing purpose", "/test2") == 0 );
//
//   // current_user = root
//   fs->current_user = root;
//   REQUIRE( fs->edit_user_entitlement("/", Node::READ_RIGHT, root->uuid) == -1 );
//   REQUIRE( fs->edit_user_entitlement("/test1", Node::READ_RIGHT, root->uuid) == -1 );
//   REQUIRE( fs->edit_user_entitlement("/test1", Node::READ_RIGHT, lambda->uuid) == 0 );
//   REQUIRE( fs->edit_user_entitlement("/test2", Node::WRITE_RIGHT, lambda->uuid) == 0 );
//   REQUIRE( fs->edit_user_entitlement("/test2", Node::WRITE_RIGHT, "10") == -ENOENT );
//   REQUIRE( fs->get_rights("/") == (Node::READ_RIGHT | Node::WRITE_RIGHT | Node::EXEC_RIGHT) );
//   REQUIRE( fs->get_rights("/test2") == (Node::READ_RIGHT | Node::WRITE_RIGHT | Node::EXEC_RIGHT) );
//
//   // current_user = lambda
//   fs->current_user = lambda;
//   REQUIRE( fs->edit_user_entitlement("/", Node::READ_RIGHT, root->uuid) == -EACCES );
//   REQUIRE( fs->edit_user_entitlement("/test1", Node::READ_RIGHT, root->uuid) == -EACCES );
//   REQUIRE( fs->edit_user_entitlement("/test1", Node::READ_RIGHT, lambda->uuid) == -EACCES );
//   REQUIRE( fs->edit_user_entitlement("/test2", Node::WRITE_RIGHT, lambda->uuid) == -EACCES );
//   REQUIRE( fs->edit_user_entitlement("/test2", Node::WRITE_RIGHT, "10") == -ENOENT );
//   REQUIRE( fs->get_rights("/") == (Node::READ_RIGHT | Node::EXEC_RIGHT) );
//   REQUIRE( fs->get_rights("/test1") == (int)Node::READ_RIGHT );
//   REQUIRE( fs->get_rights("/test2") == (int)Node::WRITE_RIGHT );
//
//   fs->current_user = root;
//   REQUIRE( fs->unlink("Testing purpose", "/test1") == 0 );
//   REQUIRE( fs->unlink("Testing purpose", "/test2") == 0 );
//   REQUIRE( read_directory(CONTENT_DIR).size() == 0 );
//   REQUIRE( read_directory(META_DIR).size() == 1 );
//   REQUIRE( read_directory(AUDIT_DIR).size() == 1 );
//
//   delete fs;
// }