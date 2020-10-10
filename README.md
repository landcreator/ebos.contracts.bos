ebos.contracts
--------------

## Version : 1.0.0

The design of the EBOS blockchain calls for a number of smart contracts that are run at a privileged permission level 
in order to support functions such as block producer registration and voting, token staking for CPU, multi-sig, etc.  
These smart contracts are referred to as the bios, system, msig, and token contracts.

This repository contains examples of these privileged contracts that are useful when deploying, managing, and/or using an EBOS blockchain.  
They are provided for reference purposes:

   * [eosio.bios](./contracts/eosio.bios)
   * [eosio.system](./contracts/eosio.system)
   * [eosio.msig](./contracts/eosio.msig)

The following unprivileged contract(s) are also part of the system.
   * [eosio.token](./contracts/eosio.token)

Dependencies:
* [ebos v1.0.x](https://github.com/landcreator/ebos/releases)
* [bos.cdt v3.0.x](https://github.com/boscore/bos.cdt/releases)