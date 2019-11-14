#!/usr/bin/env python3

from sanic import Sanic
from sanic.response import json

from lib.address    import ScriptOutput
from lib.slp        import SlpMessage

app = Sanic()

@app.route('/<script>')
def GET(request, script):
    try:
        output = ScriptOutput(bytes.fromhex(script))
        msg = SlpMessage.parseSlpOutputScript(output)

        data = {}
        data["versionType"] = msg.token_type
        data["transactionType"] = msg.transaction_type
        
        if 'token_id_hex' in msg.op_return_fields:
            data["tokenIdHex"] = msg.op_return_fields['token_id_hex']
        if 'token_output' in msg.op_return_fields:
            data["sendOutputs"] = list(map(str, msg.op_return_fields['token_output']))

        print(msg.op_return_fields)
        #ticker': b'', 'token_name': b'UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU', 'token_doc_url': b'', 'token_doc_hash
        if 'ticker' in msg.op_return_fields:
            data['symbol'] = msg.op_return_fields['ticker'].hex().upper()

        if 'token_name' in msg.op_return_fields:
            data['name'] = msg.op_return_fields['token_name'].hex().upper()

        if 'token_doc_url' in msg.op_return_fields:
            data['documentUri'] = msg.op_return_fields['token_doc_url'].hex().upper()

        if 'token_doc_hash' in msg.op_return_fields:
            data['documentSha256'] = msg.op_return_fields['token_doc_hash'].hex().upper()

        if 'decimals' in msg.op_return_fields:
            data['decimals'] = msg.op_return_fields['decimals']

        if 'mint_baton_vout' in msg.op_return_fields:
            data['batonVout'] = msg.op_return_fields['mint_baton_vout']

            if data['transactionType'] == 'MINT':
                data['containsBaton'] = data['batonVout'] is not None

        if 'initial_token_mint_quantity' in msg.op_return_fields:
            data['genesisOrMintQuantity'] = str(msg.op_return_fields['initial_token_mint_quantity'])
        if 'additional_token_quantity' in msg.op_return_fields:
            data['genesisOrMintQuantity'] = str(msg.op_return_fields['additional_token_quantity'])




        ret = json({
            'success': True,
            'data': data
        })
        return ret

    except Exception as e:
        ret = json({
            'success': False,
            'error': str(e)
        })
        return ret

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=8078)
