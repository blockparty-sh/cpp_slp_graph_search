#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <gs++/transaction.hpp>

int main(int argc, char * argv[])
{
    /*
    if (argc < 2) {
        std::cerr << "you must pass txdata" << std::endl;
        return 1;
    }
    */
    std::string txdata = "02000000028cb0b6e5ce0610eceb9d4bceb01e91f4ccbdc6a455e79d02e13dc3c407022156020000006a47304402200dde24dcf7fa41e319d18ed3dc9c72c3ec78c62bedbdfce9137e8a0dc873673e0220470d0b71cb50f2aaca70387753982e515c03af4818cf3db8b7f66034c735807841210350090260acd0cd7a5f7030b2aad76ec6454626ab0872246031f3809d210e4569ffffffff8cb0b6e5ce0610eceb9d4bceb01e91f4ccbdc6a455e79d02e13dc3c407022156030000006a47304402202e2a5558007f71eabab850af09382791d0d1b269a8b752c071e7857094dc09de022019356b5217b8a946cf9c0c4e87983636e359c0ef1a163e846e3165d98039d2b841210350090260acd0cd7a5f7030b2aad76ec6454626ab0872246031f3809d210e4569ffffffff040000000000000000406a04534c500001010453454e44207f8889682d57369ed0e32336f8b7e0ffec625a35cca183f4e81fde4e71a538a10800000000000017bf080000000010bad3e422020000000000001976a91442ae47fda0f9d68464f2a2e003ea3913908eddcf88ac22020000000000001976a9144942f11b739a3835d867554ff93cc6685eb1eb5388ac7faa3b00000000001976a9144942f11b739a3835d867554ff93cc6685eb1eb5388ac00000000";

    const std::vector<std::uint8_t> txhex = gs::util::unhex(txdata);
    gs::transaction tx;
    if (! tx.hydrate(txhex.begin(), txhex.end()) ) {
        std::cerr << "tx hydration failed" << std::endl;
        return 1;
    }

    std::cout << tx << std::endl;

    return 0;
}
