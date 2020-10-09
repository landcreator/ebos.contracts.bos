#include <eosio.system/eosio.system.hpp>

#include <eosio.token/eosio.token.hpp>

#include <vector>
namespace eosiosystem {

   void system_contract::onblock( ignore<block_header> ) {
      using namespace eosio;

      require_auth(_self);

      block_timestamp timestamp;
      name producer;
      _ds >> timestamp >> producer;

      /// only update block producers once every minute, block_timestamp is in half seconds
      if (timestamp.slot - _gstate.last_producer_schedule_update.slot > 120) {
         update_elected_producers(timestamp);
      }
   }

   using namespace eosio;
   void system_contract::claimrewards( const name owner ) {
      check( false, "claimrewards is not support on this chain");
   }

} //namespace eosiosystem
