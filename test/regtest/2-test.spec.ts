/* eslint-disable @typescript-eslint/no-unsafe-member-access */
/* eslint-disable @typescript-eslint/no-floating-promises */
/* eslint-disable @typescript-eslint/no-misused-promises */
import { step } from 'mocha-steps';
import * as assert from "assert";
import * as zmq from 'zeromq';
import { GraphSearchClient } from "grpc-graphsearch-node";
import { BigNumber } from "bignumber.js";
import { BitcoinRpcClient, RpcWalletClient, sleep, validityCache } from './helpers/rpcwallet';
import { Crypto } from "slp-validate";

// rpc clients for talking to the two full nodes
const rpcClient = require('bitcoin-rpc-promise');  // TODO: create a new typed version of this library with SLPDB patch applied for missing commands
const bchnRpc1 = new rpcClient('http://bitcoin:password@0.0.0.0:18443') as BitcoinRpcClient;
const bchnRpc2 = new rpcClient('http://bitcoin:password@0.0.0.0:18444') as BitcoinRpcClient;

// graphsearch client (will be connecting to bitcoind1, see docker-compose.yml)
const gsGrpc = new GraphSearchClient({ url: "localhost:50051", notls: true });

// stubs for the different wallets.
let wallet1: RpcWalletClient;
let wallet2: RpcWalletClient;

// stubs for slp token info
let tokenId1: string;
let tokenId2: string;

// gs++ zmq tx/blk notifications
const gsZmqTxns = new Set<string>();
const gsZmqBlocks = new Set<string>();

// connect to gs++ ZMQ notifications
const sock = zmq.socket('sub');
sock.connect('tcp://127.0.0.1:29069');
sock.subscribe('rawtx');
sock.subscribe('rawblock');
console.log("Subscriber connected to gs++");

// eslint-disable-next-line @typescript-eslint/require-await
sock.on('message', (topic: string, message: Buffer) => {
    if (topic.toString() === 'rawtx') {
        gsZmqTxns.add(Crypto.HashTxid(message).toString("hex"));
    } else if (topic.toString() === 'rawblock') {
        // nothing yet...
    }
});

// TODO?: setup async randomized block generation for the two full nodes using setTimeout.

let slpTxnCount = 0;
const WAIT_FOR_GS_ZMQ = async () => {
    console.time("gs++ zmq");
    slpTxnCount++;
    while (gsZmqTxns.size !== slpTxnCount) {
        await sleep(100);
    }
    console.timeEnd("gs++ zmq");
}

describe("network health check", () => {
    step("bitcoind1 ready", async () => {
        const info = await bchnRpc1.getBlockchainInfo();
        assert.strictEqual(info.chain, "regtest");
    });
    step("gs++ ready (connected to bitcoind1)", async () => {
        const status = await gsGrpc.getStatus();
        const height = status.getBlockHeight();
        assert.ok(height >= 0);
    });
    step("bitcoind2 ready", async () => {
        const info = await bchnRpc2.getBlockchainInfo();
        assert.strictEqual(info.chain, "regtest");
    });
    step("bitcoind1 and bitcoind2 are connected", async () => {
        let peerInfo1 = await bchnRpc1.getPeerInfo();
        if (peerInfo1.length < 1) {
            await bchnRpc1.addNode("bitcoind2", "onetry");
            while (peerInfo1.length < 1) {
                await sleep(100);
                peerInfo1 = await bchnRpc1.getPeerInfo();
            }
        }
        assert.strictEqual(peerInfo1.length, 1);

        const peerInfo2 = await bchnRpc2.getPeerInfo();
        assert.strictEqual(peerInfo2.length, 1);
    });
});

describe("basic wallet setup", () => {
    step("setup wallet 1 (at bitcoind1)", async () => {
        wallet1 = await RpcWalletClient.CreateRegtestWallet(bchnRpc1);
        let bal = await wallet1.getAllUnspent(false);
        assert.ok(bal.length > 0);

        wallet2 = await RpcWalletClient.CreateRegtestWallet(bchnRpc2);
        bal = await wallet2.getAllUnspent(false);
        assert.ok(bal.length > 0);
    });
    step("submit an slp genesis transaction", async () => {
        tokenId1 = await wallet1.slpGenesis();
        await WAIT_FOR_GS_ZMQ();
        let gs = await gsGrpc.trustedValidationFor({ hash: tokenId1, reversedHashOrder: true });
        assert.strictEqual(gs.getValid(), true);

        tokenId2 = await wallet2.slpGenesis();
        await WAIT_FOR_GS_ZMQ(); // Requires > 1 sec delay in order be valid in gs++!!
        gs = await gsGrpc.trustedValidationFor({ hash: tokenId2, reversedHashOrder: true });
        assert.strictEqual(gs.getValid(), true);
    });
    step("submit an slp mint transaction", async () => {
        let txid = await wallet1.slpMint(tokenId1, { address: wallet1.address, amount: new BigNumber(100) }, 2);
        await WAIT_FOR_GS_ZMQ();
        let gs = await gsGrpc.trustedValidationFor({ hash: txid, reversedHashOrder: true });
        assert.strictEqual(gs.getValid(), true);

        txid = await wallet2.slpMint(tokenId2, { address: wallet2.address, amount: new BigNumber(100) }, 2);
        await WAIT_FOR_GS_ZMQ();
        gs = await gsGrpc.trustedValidationFor({ hash: txid, reversedHashOrder: true });
        assert.strictEqual(gs.getValid(), true);
    });
    step("submit an slp send transaction", async () => {
        let txid = await wallet1.slpSend(tokenId1, [{ address: wallet1.address, tokenAmount: new BigNumber(1) }]);
        await WAIT_FOR_GS_ZMQ();
        let gs = await gsGrpc.trustedValidationFor({ hash: txid, reversedHashOrder: true });
        assert.strictEqual(gs.getValid(), true);

        txid = await wallet2.slpSend(tokenId2, [{ address: wallet2.address, tokenAmount: new BigNumber(1) }]);
        await WAIT_FOR_GS_ZMQ();
        gs = await gsGrpc.trustedValidationFor({ hash: txid, reversedHashOrder: true });
        assert.strictEqual(gs.getValid(), true);
    });

    const mint = async (wallet: RpcWalletClient, tokenId: string, dagCount: number|null): Promise<number> => {

        // mint slp token
        console.time("mint");
        const txid = await wallet.slpMint(tokenId, {address: {cashAddress: wallet._miningAddress, slpAddress: wallet._miningAddress}, amount: new BigNumber(100)} , 2);
        console.timeEnd("mint");
        await WAIT_FOR_GS_ZMQ();

        // check gs++ validity
        console.time("gs++");
        const gs1 = await gsGrpc.trustedValidationFor({ hash: txid, reversedHashOrder: true });
        assert.strictEqual(gs1.getValid(), true);
        validityCache.add(txid);
        console.timeEnd("gs++");

        // check gs++ graph search length
        const gs2 = await gsGrpc.graphSearchFor({ hash: txid, reversedHashOrder: true });
        if (! dagCount) {
            dagCount = gs2.getTxdataList_asU8().length;
        } else {
            assert.strictEqual(dagCount, gs2.getTxdataList_asU8().length);
        }

        return dagCount;
    };

    step("long minting string returns proper graph search results, 1 mint per block", async () => {
        let dagCount1: number|null = null;
        let dagCount2: number|null = null;

        for (let i = 0; i < 10000; i++) {
            dagCount1 = await mint(wallet1, tokenId1, dagCount1);
            dagCount2 = await mint(wallet2, tokenId2, dagCount2);

            // mine the mint txn into a block
            console.time("generate");
            await bchnRpc1.generateToAddress(1, wallet1._miningAddress);
            console.timeEnd("generate");

            // ...
            console.log(`dag1 size ${dagCount1}`);
            console.log(`dag2 size ${dagCount2}`);
        }
    });
});
