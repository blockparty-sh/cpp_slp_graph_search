#ifndef GS_HPP
#define GS_HPP

#include <boost/filesystem.hpp>
#include <gs++/bhash.hpp>
#include <gs++/gs_tx.hpp>
#include <gs++/txgraph.hpp>

boost::filesystem::path get_tokendir(const gs::tokenid tokenid);
void signal_handler(int signal);

// TODO save writes into buffer to prevent many tiny writes
// should improve performance
bool save_token_to_disk(gs::txgraph & g, const gs::tokenid tokenid);

std::vector<gs::gs_tx> load_token_from_disk(gs::txgraph & g, const gs::tokenid tokenid);

#endif
