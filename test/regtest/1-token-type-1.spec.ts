import * as zmq from 'zeromq';
import { step } from 'mocha-steps';
import BigNumber from 'bignumber.js';



const rpcClient = require('bitcoin-rpc-promise-retry');
const connectionStringNode1_miner = 'http://bitcoin:password@0.0.0.0:18443';  // (optional) connect to a miner's rpc on 18444 that is not connected to SLPDB
const rpcNode1_miner = new rpcClient(connectionStringNode1_miner, { maxRetries: 0 });

// connect to SLPDB ZMQ notifications
const sock: any = zmq.socket('sub');
sock.connect('tcp://0.0.0.0:27339');
sock.subscribe('mempool');
sock.subscribe('block');
sock.on('message', async function(topic: string, message: Buffer) {
	console.log(message);
});


let receiverRegtest: string;
describe("1-Token-Type-1", () => {
    step("Initial setup for all tests", async () => {
        // TODO: burn any existing wallet funds, in order to prevent "Transaction too large".

        // generate block to clear the mempool (may be dirty from previous tests)
        await rpcNode1_miner.generate(1);
        
        // make sure we have coins to use in tests
        let balance = await rpcNode1_miner.getBalance();
        while (balance < 1) {
            await rpcNode1_miner.generate(1);
            balance = await rpcNode1_miner.getBalance();
        }

        // put all the funds on the receiver's address
        receiverRegtest = await rpcNode1_miner.getNewAddress("0");
        await rpcNode1_miner.sendToAddress(receiverRegtest, 1, "", "", true);
    });
	step("Cleanup after tests", async () => {
        // generate block to clear the mempool (may be dirty from previous tests)
        await rpcNode1_miner.generate(1);
        sock.disconnect('tcp://0.0.0.0:27339');
    });
});
