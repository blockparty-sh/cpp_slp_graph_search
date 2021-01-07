#include <cstdint>
#include <stdio.h>

// this is the size of the data (in bytes) which is attached to outputs
// we read from inputs in this size, and write to outputs this size
constexpr int UTXO_MDATA_SIZE = 64;

// memory locations that are set up by the vm
// these are populated with data from the transaction
/*
constexpr uintptr_t MEM_LOC_INPUTS   = 0x740000000000;
constexpr uintptr_t MEM_LOC_USER     = 0x741000000000;
constexpr uintptr_t MEM_LOC_OUTPUTS  = 0x742000000000;
*/
constexpr uintptr_t MEM_LOC_INPUTS   = 0x7fff54a82e48;
constexpr uintptr_t MEM_LOC_USER     = 0x7fff54a82e48;
constexpr uintptr_t MEM_LOC_OUTPUTS  = 0x7fff54a82e48;

// how many inputs are there
volatile inline uint32_t inputs_size()
{
    return *reinterpret_cast<uint32_t*>(MEM_LOC_INPUTS);
}

// extract data from input
template <typename T>
volatile inline T read_input_data(const int idx, const int offset)
{
    constexpr uintptr_t MEM_LOC_INPUTS_DATA = MEM_LOC_INPUTS + sizeof(uint32_t);
    return *reinterpret_cast<T*>(MEM_LOC_INPUTS_DATA + (UTXO_MDATA_SIZE * idx) + offset);
}

// length of the user input in bytes, see below for more info
volatile inline uint32_t user_data_size()
{
    return *reinterpret_cast<uint32_t*>(MEM_LOC_USER);
}

// extract data from user input
// this is what comes after the SEND part in the OP_RETURN message in the tx
// maybe this should be more abstracted or implemented differently but just e.g.
template <typename T>
volatile inline T read_user_data(const int offset)
{
    constexpr uintptr_t MEM_LOC_USER_DATA = MEM_LOC_USER + sizeof(uint32_t);
    return *reinterpret_cast<T*>(MEM_LOC_USER_DATA + offset);
}

// how many outputs there are in the transaction
volatile inline uint32_t outputs_size()
{
    return *reinterpret_cast<uint32_t*>(MEM_LOC_OUTPUTS);
}

// this is how we set the output data to propagate across multiple transactions
template <typename T>
volatile inline void write_output_data(const int idx, const int offset, const T data)
{
    constexpr uintptr_t MEM_LOC_OUTPUTS_DATA = MEM_LOC_OUTPUTS + sizeof(uint32_t);
    *reinterpret_cast<T*>(MEM_LOC_OUTPUTS_DATA + (UTXO_MDATA_SIZE * idx) + offset) = data;
}

/*
 * our UTXO_MDATA looks like this
 * 0x00: balance (uint64)
 *
 * we want to look at all of the inputs and gather the total of their balances
 * we then look at what the user has provided for input data
 * the format for the user data is:
 * balance, balance, balance...
 * where each balance is uint64
 * we then write the new data to the outputs
 */
int main(int argc, char * argv[])
{
    // first check that the user data is correct size
    // we want multiples of 8 bytes
    const uint32_t udata_len = user_data_size();
    if (udata_len % sizeof(uint64_t) != 0) {
        return 1;
    }

    // we know we have this many balances given
    const uint32_t given_balances_size = udata_len / sizeof(uint64_t);

    // if there are not enough outputs for balances given we exit early
    if (outputs_size() < given_balances_size) {
        return 1;
    }

    // we use uint128_t here to prevent overflow from adding uint64s together
    __uint128_t input_balance  = 0;
    __uint128_t output_balance = 0;

    // read each of the UTXO_MDATA for inputs 
    // we use 0 as an offset to fit with our defined data structure described above main
    for (uint32_t i=0; i<inputs_size(); ++i) {
        input_balance += read_input_data<uint64_t>(i, 0x0);
    }

    // we populate this array to later apply to the outputs after validation
    uint64_t given_balances[given_balances_size];
    for (uint32_t i=0; i<given_balances_size; ++i) {
        given_balances[i] = read_user_data<uint64_t>(i*sizeof(uint64_t));
        output_balance += given_balances[i];
    }

    // self explanatory :)
    // if someone is trying to inflate the money supply, burn their funds
    if (input_balance < output_balance) {
        return 1;
    }

    // write the balance data back to UTXO_MDATA
    for (uint32_t i=0; i<given_balances_size; ++i) {
        write_output_data(i, 0x0, given_balances[i]);
    }

    return 0;
}
