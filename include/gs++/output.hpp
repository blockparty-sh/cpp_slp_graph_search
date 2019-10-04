#ifndef GS_OUTPOINT_HPP
#define GS_OUTPOINT_HPP

#include <cstdint>
#include <vector>
#include <gs++/bhash.hpp>
#include <gs++/pk_script.hpp>
#include <absl/hash/hash.h>


namespace gs {

struct output
{
	gs::txid      prev_tx_id;
	std::uint32_t prev_out_idx;
	std::uint32_t height;
	std::uint64_t value;
    gs::pk_script pk_script;

	output(){}

	output (
		const gs::txid      prev_tx_id,
		const std::uint32_t prev_out_idx,
		const std::uint32_t height,
		const std::uint64_t value,
		const gs::pk_script pk_script
	)
	: prev_tx_id(prev_tx_id)
	, prev_out_idx(prev_out_idx)
	, height(height)
	, value(value)
	, pk_script(pk_script)
	{}

    bool is_op_return() const
    {
        return pk_script.v[0] == 0x6a;
    }
};

struct outpoint
{
	gs::txid      txid;
	std::uint32_t vout;

	outpoint(){}

	outpoint(
		const gs::txid      txid,
		const std::uint32_t vout
	)
	: txid(txid)
	, vout(vout)
	{}

    bool operator==(const outpoint &o) const
    { return txid == o.txid && vout == o.vout; }

    template <typename H>
    friend H AbslHashValue(H h, const outpoint& m)
    {
        return H::combine(std::move(h), m.txid, m.vout);
    }
};


}

#endif
