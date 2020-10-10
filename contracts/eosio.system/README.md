eosio.system
----------

This contract provides multiple functionalities:
- Users can stake tokens for CPU, and then vote for producers.
- Producers register in order to be voted for.

Actions:
The naming convention is codeaccount::actionname followed by a list of paramters.

## eosio::init( symbol core )
   - **core** core symbol

## eosio::void setacntfee( asset account_creation_fee )
   - **account_creation_fee** account creation fee
   
## eosio::setvweight( uint32_t company_weight, uint32_t government_weight )
   - **company_weight** company account vote weight
   - **government_weight** government account vote weight

## eosio::void awlset( string action, name account )
   - account white list, only account added can deploy smart contract
   - **action** "add" or "delete"
   - **account** account name

## eosio::regproducer producer producer_key url location
   - Indicates that a particular account wishes to become a producer
   - **producer** account registering to be a producer candidate
   - **producer_key** producer account public key
   - **url** producer URL
   - **location** currently unused index

## eosio::voteproducer voter proxy producers
   - **voter** the account doing the voting
   - **proxy** ***not used*** proxy account to whom voter delegates vote
   - **producers** list of producers voted for. A maximum of 30 producers is allowed
   - Voter can vote for a proxy __or__ a list of at most 30 producers. Storage change is billed to `voter`.

## eosio::delegatebw from receiver stake\_net\_quantity stake\_cpu\_quantity transfer
   - **from** account holding tokens to be staked
   - **receiver** account to whose resources staked tokens are added
   - **stake\_net\_quantity** ***must be zero asset*** tokens staked for NET bandwidth
   - **stake\_cpu\_quantity** tokens staked for CPU bandwidth
   - **transfer** if true, ownership of staked tokens is transfered to `receiver`
   - All producers `from` account has voted for will have their votes updated immediately.

## eosio::dlgtcpu( name from, name receiver, asset stake_cpu_quantity, bool transfer )
   - directly call **delegatebw** internally, which with stake_net_quantity be zero asset

## eosio::undelegatebw from receiver unstake\_net\_quantity unstake\_cpu\_quantity
   - **from** account whose tokens will be unstaked
   - **receiver** account to whose benefit tokens have been staked
   - **unstake\_net\_quantity** ***must be zero asset*** tokens to be unstaked from NET bandwidth
   - **unstake\_cpu\_quantity** tokens to be unstaked from CPU bandwidth
   - Unstaked tokens are transferred to `from` liquid balance via a deferred transaction with a delay of 3 days.
   - If called during the delay period of a previous `undelegatebw` action, pending action is canceled and timer is reset.
   - All producers `from` account has voted for will have their votes updated immediately.
   - Bandwidth and storage for the deferred transaction are billed to `from`.

## eosio::undlgtcpu( name from, name receiver, asset unstake_cpu_quantity )
   - directly call **undelegatebw** internally, which with unstake_net_quantity be zero asset

## eosio::onblock header
   - This special action is triggered when a block is applied by a given producer, and cannot be generated from
     any other source. It is used increment the number of unpaid blocks by a producer and update producer schedule.
