#include "../utils/metadata.hpp"
#include "../utils/encryption.hpp"

#include <cerrno>
#include <string>
#include <cstring>
#include <vector>



///////////////////////////////////////////////////////////////////////////////
////////////////////////////    Filenode     //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
Filenode::Filenode(const std::string &filename, const size_t block_size) {
  this->filename = filename;
  this->block_size = block_size;

  this->plain = new std::vector<std::vector<char>*>();
  this->cipher = new std::vector<std::vector<char>*>();
  this->aes_ctr_ctxs = new std::vector<AES_CTR_context*>();
}

Filenode::~Filenode() {
  for (auto it = this->plain->begin(); it != this->plain->end(); ) {
    delete * it;
    it = this->plain->erase(it);
  }
  delete this->plain;

  for (auto it = this->cipher->begin(); it != this->cipher->end(); ) {
    delete * it;
    it = this->cipher->erase(it);
  }
  delete this->cipher;


  for (auto it = this->aes_ctr_ctxs->begin(); it != this->aes_ctr_ctxs->end(); ) {
    delete * it;
    it = this->aes_ctr_ctxs->erase(it);
  }
  delete this->aes_ctr_ctxs;
}


size_t Filenode::size() {
  if (this->plain->empty())
    return 0;

  size_t size = (this->plain->size() - 1) * this->block_size;
  size += this->plain->back()->size();
  return size;
}

size_t Filenode::write(const long offset, const size_t data_size, const char *data) {
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
    Filenode::encrypt_block(block_index);
    written += bytes_to_write;
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
    Filenode::encrypt_block(this->plain->size()-1);
    written += bytes_to_write;
  }

  return written;
}

size_t Filenode::read(const long offset, const size_t buffer_size, char *buffer) {
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


size_t Filenode::metadata_size() {
  return AES_CTR_context::size() * this->aes_ctr_ctxs->size();
}

size_t Filenode::dump_metadata(const size_t buffer_size, char *buffer) {
  size_t written = 0, ctx_size=AES_CTR_context::size();
  for (size_t index = 0; index < this->aes_ctr_ctxs->size() && written+ctx_size <= buffer_size; index++) {
    AES_CTR_context *context = this->aes_ctr_ctxs->at(index);
    written += context->dump(written, buffer);
  }

  return written;
}

size_t Filenode::load_metadata(const size_t buffer_size, const char *buffer) {
  size_t read = 0, ctx_size=AES_CTR_context::size();
  for (size_t index = 0; read+ctx_size <= buffer_size; index++) {
    AES_CTR_context *context = new AES_CTR_context((uint8_t*)buffer+read, (uint8_t*)buffer+read+16);
    this->aes_ctr_ctxs->push_back(context);
    read += ctx_size;
  }

  return read;
}


size_t Filenode::encryption_size(const long up_offset, const size_t up_size) {
  size_t written = 0;
  size_t offset_in_block = up_offset % this->block_size;
  size_t start_index = (size_t)((up_offset-offset_in_block)/this->block_size);
  offset_in_block = (up_offset+up_size) % this->block_size;
  size_t end_index = (size_t)((up_offset+up_size-offset_in_block)/this->block_size);

  if (offset_in_block > 0)
    return (end_index-start_index) * this->block_size + this->cipher->at(end_index)->size();
  return (end_index-start_index) * this->block_size;
}

size_t Filenode::dump_encryption(const long up_offset, const size_t up_size, const size_t buffer_size, char *buffer) {
  size_t written = 0;
  size_t offset_in_block = up_offset % this->block_size;
  size_t start_index = (size_t)((up_offset-offset_in_block)/this->block_size);

  if (this->cipher->size() <= start_index)
    return -1;

  std::vector<char> *block = this->cipher->at(start_index);
  for (size_t index = start_index; written+block->size() <= buffer_size; index++) {
    std::memcpy(buffer + written, &(*block)[0], block->size());
    written += block->size();
    if (this->cipher->size() <= index + 1)
      break;
    block = this->cipher->at(index + 1);
  }

  return start_index * this->block_size; // return the offset on which it should editing the file
}

size_t Filenode::load_encryption(const long offset, const size_t buffer_size, const char *buffer) {
  size_t block_index = (size_t)(offset/this->block_size);

  std::vector<char> *block = new std::vector<char>(buffer_size);
  std::memcpy(&(*block)[0], buffer, buffer_size);
  this->cipher->push_back(block);
  Filenode::decrypt_block(block_index);
  return buffer_size;
}

size_t Filenode::encrypt_block(const size_t block_index) {
  std::vector<char> *plain_block = this->plain->at(block_index);
  std::vector<char> *cipher_block = this->cipher->at(block_index);
  if (block_index < this->aes_ctr_ctxs->size()) // already exists
    this->cipher->at(block_index)->resize(plain_block->size());
  else
    this->cipher->push_back(new std::vector<char>(plain_block->size()));

  AES_CTR_context *ctx = this->aes_ctr_ctxs->at(block_index);
  return ctx->encrypt((uint8_t*)&(*plain_block)[0], plain_block->size(), (uint8_t*)&(*cipher_block)[0]);
}

size_t Filenode::decrypt_block(const size_t block_index) {
  std::vector<char> *cipher_block = this->cipher->at(block_index);
  AES_CTR_context *ctx = this->aes_ctr_ctxs->at(block_index);

  if (block_index < this->plain->size()) // already exists
    this->plain->at(block_index)->resize(cipher_block->size());
  else
    this->plain->push_back(new std::vector<char>(cipher_block->size()));

  std::vector<char> *plain_block = this->plain->at(block_index);
  return ctx->decrypt((uint8_t*)&(*cipher_block)[0], cipher_block->size(), (uint8_t*)&(*plain_block)[0]);
}
