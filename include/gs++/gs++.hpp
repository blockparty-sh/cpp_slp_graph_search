#ifndef GS_HPP
#define GS_HPP

#include <filesystem>
#include "txhash.hpp"

std::filesystem::path get_tokendir(const txhash tokenid);
void signal_handler(int signal);

#endif
