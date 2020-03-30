#include "../../utils/metadata/filenode.hpp"
#include "../../utils/metadata/supernode.hpp"
#include "../../utils/metadata/node.hpp"
#include "../../utils/encryption.hpp"
#include "../../utils/users/user.hpp"

#include <cerrno>
#include <string>
#include <cstring>
#include <map>
#include <vector>



Filenode::Filenode(const std::string &filename, AES_GCM_context *root_key,
                    const size_t block_size):Node::Node(filename, root_key) {
  this->block_size = block_size;
  this->allowed_users = new std::map<User*, unsigned char>();

  this->plain = new std::vector<std::vector<char>*>();
  this->cipher = new std::vector<std::vector<char>*>();
  this->aes_ctr_ctxs = new std::vector<AES_CTR_context*>();
}

Filenode::~Filenode() {
  for (auto it = this->plain->begin(); it != this->plain->end(); ) {
    delete * it; it = this->plain->erase(it);
  }

  for (auto it = this->cipher->begin(); it != this->cipher->end(); ) {
    delete * it; it = this->cipher->erase(it);
  }

  for (auto it = this->aes_ctr_ctxs->begin(); it != this->aes_ctr_ctxs->end(); ) {
    delete * it; it = this->aes_ctr_ctxs->erase(it);
  }

  delete this->plain; delete this->cipher;
  delete this->aes_ctr_ctxs; delete this->allowed_users;
}


bool Filenode::is_user_allowed(const unsigned char required_policy, User *user) {
  if (user->is_root())
    return true;

  auto it = this->allowed_users->find(user);
  if (it == this->allowed_users->end())
    return false;

  unsigned char policy = it->second;
  return required_policy == (required_policy & policy);
}

int Filenode::edit_user_policy(const unsigned char policy, User *user) {
  if (user->is_root())
    return -1;

  unsigned char effective_policy = policy;
  if (policy == Filenode::OWNER_POLICY)
    effective_policy = Filenode::OWNER_POLICY | Filenode::READ_POLICY | Filenode::WRITE_POLICY | Filenode::EXEC_POLICY;

  auto it = this->allowed_users->find(user);
  if (it == this->allowed_users->end() && policy != 0) {
    this->allowed_users->insert(std::pair<User*, unsigned char>(user, effective_policy));
    return 0;
  }

  if (policy == 0)
    this->allowed_users->erase(it);
  else
    it->second = policy;
  return 0;
}


size_t Filenode::size() {
  if (this->plain->empty())
    return 0;

  size_t size = (this->plain->size() - 1) * this->block_size;
  size += this->plain->back()->size();
  return size;
}

int Filenode::write(const long offset, const size_t data_size, const char *data) {
  size_t written = 0;
  size_t offset_in_block = offset % block_size;
  size_t block_index = (size_t)((offset-offset_in_block)/this->block_size);

  // fill as much as we can inside available blocks
  if (block_index < this->plain->size()) {
    std::vector<char> *block = this->plain->at(block_index);
    size_t bytes_to_write = data_size;
    if (this->block_size < (offset_in_block + data_size)) {
      bytes_to_write = this->block_size - offset_in_block;
      block->resize(this->block_size);
    } else {
      block->resize(offset_in_block + data_size);
    }
    std::memcpy(&(*block)[0] + offset_in_block, data, bytes_to_write);
    written += bytes_to_write;

    if (Filenode::encrypt_block(block_index) < 0)
      return -1;
  }

  // Create new blocks from scratch for extra
  while (written < data_size) {
    size_t bytes_to_write = data_size - written;
    if (this->block_size < bytes_to_write) {
      bytes_to_write = this->block_size;
    }
    std::vector<char> *block = new std::vector<char>(bytes_to_write);
    std::memcpy(&(*block)[0], data + written, bytes_to_write);
    this->plain->push_back(block);
    this->aes_ctr_ctxs->push_back(new AES_CTR_context());
    written += bytes_to_write;

    if (Filenode::encrypt_block(this->plain->size()-1) < 0)
      return -1;
  }

  return written;
}

int Filenode::read(const long offset, const size_t buffer_size, char *buffer) {
  size_t read = 0;
  size_t offset_in_block = offset % this->block_size;
  size_t block_index = (size_t)((offset-offset_in_block)/this->block_size);

  if (this->plain->size() <= block_index)
    return 0;

  for (size_t index = block_index;
       index < this->plain->size() && read < buffer_size;
       index++, offset_in_block = 0) {
    std::vector<char> *block = this->plain->at(index);
    auto size_to_copy = buffer_size - read;
    if (size_to_copy > block->size()) {
      size_to_copy = block->size();
    }
    std::memcpy(buffer + read, &(*block)[0] + offset_in_block, size_to_copy);
    read += size_to_copy;
  }

  return (int)read;
}


size_t Filenode::size_sensitive() {
  size_t size = 2 * sizeof(int);
  size += this->allowed_users->size() * (sizeof(int) + sizeof(unsigned char));
  return size + AES_CTR_context::size() * this->aes_ctr_ctxs->size();
}

int Filenode::dump_sensitive(const size_t buffer_size, char *buffer) {
  size_t written = 0;
  size_t users_len = this->allowed_users->size(), keys_len = this->aes_ctr_ctxs->size();

  std::memcpy(buffer, &users_len, sizeof(int)); written += sizeof(int);
  for (auto it = this->allowed_users->begin(); it != this->allowed_users->end(); ++it) {
    int user_id = it->first->id;
    unsigned char policy = it->second;

    std::memcpy(buffer+written, &user_id, sizeof(int)); written += sizeof(int);
    std::memcpy(buffer+written, &policy, sizeof(unsigned char)); written += sizeof(unsigned char);
  }

  std::memcpy(buffer+written, &keys_len, sizeof(int)); written += sizeof(int);
  for (size_t index = 0; index < keys_len; index++) {
    AES_CTR_context *context = this->aes_ctr_ctxs->at(index);
    size_t step = context->dump(buffer+written);
    if (step < 0)
      return -1;
    written += step;
  }
  return written;
}

int Filenode::load_sensitive(Node *parent, const size_t buffer_size, const char *buffer) {
  size_t read = 0, users_len = 0, keys_len = 0;

  std::memcpy(&users_len, buffer, sizeof(int)); read += sizeof(int);
  for (size_t index = 0; index < users_len; index++) {
    int user_id = 0;
    unsigned char policy = 0;

    std::memcpy(&user_id, buffer+read, sizeof(int)); read += sizeof(int);
    std::memcpy(&policy, buffer+read, sizeof(unsigned char)); read += sizeof(unsigned char);

    User *user = ((Supernode*)parent)->retrieve_user(user_id);
    this->allowed_users->insert(std::pair<User*, unsigned char>(user, policy));
  }

  std::memcpy(&keys_len, buffer+read, sizeof(int)); read += sizeof(int);
  for (size_t index = 0; index <= keys_len; index++) {
    AES_CTR_context *context = new AES_CTR_context();
    size_t step = context->load(buffer+read);
    if (step < 0)
      return -1;
    read += step;

    this->aes_ctr_ctxs->push_back(context);
  }
  return read;
}


int Filenode::encryption_size(const long up_offset, const size_t up_size) {
  size_t written = 0;
  size_t offset_in_block = up_offset % this->block_size;
  size_t start_index = (size_t)((up_offset-offset_in_block)/this->block_size);
  offset_in_block = (up_offset+up_size) % this->block_size;
  size_t end_index = (size_t)((up_offset+up_size-offset_in_block)/this->block_size);

  if (offset_in_block > 0)
    return (end_index-start_index) * this->block_size + this->cipher->at(end_index)->size();
  return (end_index-start_index) * this->block_size;
}

int Filenode::dump_encryption(const long up_offset, const size_t up_size, const size_t buffer_size, char *buffer) {
  size_t written = 0;
  size_t offset_in_block = up_offset % this->block_size;
  size_t start_index = (size_t)((up_offset-offset_in_block)/this->block_size);

  if (this->cipher->size() <= start_index)
    return -1;

  std::vector<char> *block = this->cipher->at(start_index);
  for (size_t index = start_index; written+block->size() <= buffer_size; index++) {
    std::memcpy(buffer + written, &(*block)[0], block->size());
    written += block->size();
    if (this->cipher->size() <= index+1)
      break;
    block = this->cipher->at(index+1);
  }

  return start_index * this->block_size; // return the offset on which it should start editing the file
}

int Filenode::load_encryption(const long offset, const size_t buffer_size, const char *buffer) {
  size_t block_index = (size_t)(offset/this->block_size);

  std::vector<char> *block = new std::vector<char>(buffer_size);
  std::memcpy(&(*block)[0], buffer, buffer_size);
  this->cipher->push_back(block);
  return Filenode::decrypt_block(block_index);
}

int Filenode::encrypt_block(const size_t block_index) {
  std::vector<char> *plain_block = this->plain->at(block_index);
  AES_CTR_context *ctx = this->aes_ctr_ctxs->at(block_index);
  if (block_index < this->cipher->size()) // already exists
    this->cipher->at(block_index)->resize(plain_block->size());
  else
    this->cipher->push_back(new std::vector<char>(plain_block->size()));

  std::vector<char> *cipher_block = this->cipher->at(block_index);
  return ctx->encrypt((uint8_t*)&(*plain_block)[0], plain_block->size(), (uint8_t*)&(*cipher_block)[0]);
}

int Filenode::decrypt_block(const size_t block_index) {
  std::vector<char> *cipher_block = this->cipher->at(block_index);
  AES_CTR_context *ctx = this->aes_ctr_ctxs->at(block_index);

  if (block_index < this->plain->size()) // already exists
    this->plain->at(block_index)->resize(cipher_block->size());
  else
    this->plain->push_back(new std::vector<char>(cipher_block->size()));

  std::vector<char> *plain_block = this->plain->at(block_index);
  return ctx->decrypt((uint8_t*)&(*cipher_block)[0], cipher_block->size(), (uint8_t*)&(*plain_block)[0]);
}