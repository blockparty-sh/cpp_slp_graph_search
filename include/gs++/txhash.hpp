#ifndef GS_TXHASH_HPP
#define GS_TXHASH_HPP

#include <string>
#include <cassert>

using txhash = std::string;

txhash compress_txhash(const std::string & hex);
txhash decompress_txhash(const txhash & hash);

#endif
