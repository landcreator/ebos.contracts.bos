#include <eosio.system/eosio.system.hpp>
#include <eosiolib/dispatcher.hpp>
#include <eosiolib/crypto.h>

#include "producer_pay.cpp"
#include "delegate_bandwidth.cpp"
#include "voting.cpp"
#include <map>
#include "upgrade.cpp"

namespace eosiosystem {

   system_contract::system_contract( name s, name code, datastream<const char*> ds )
   :native(s,code,ds),
    _voters(_self, _self.value),
    _producers(_self, _self.value),
    _producers2(_self, _self.value),
    _global(_self, _self.value),
    _global2(_self, _self.value),
    _global3(_self, _self.value),
    _guarantee(_self, _self.value),
    _upgrade(_self, _self.value)
   {
      _gstate  = _global.exists() ? _global.get() : get_default_parameters();
      _gstate2 = _global2.exists() ? _global2.get() : eosio_global_state2{};
      _gstate3 = _global3.exists() ? _global3.get() : eosio_global_state3{};
      _ustate = _upgrade.exists() ? _upgrade.get() : upgrade_state{};
   }

   eosio_global_state system_contract::get_default_parameters() {
      eosio_global_state dp;
      get_blockchain_parameters(dp);
      return dp;
   }

   symbol system_contract::core_symbol()const {
      return _gstate2.core_symbol;
   }

   system_contract::~system_contract() {
      _global.set( _gstate, _self );
      _global2.set( _gstate2, _self );
      _global3.set( _gstate3, _self );
      _upgrade.set( _ustate, _self );
   }

   void system_contract::setram( uint64_t max_ram_size ) {
      require_auth( _self );
      check( _gstate.max_ram_size < max_ram_size, "ram may only be increased" );
      check( max_ram_size < 1024ll*1024*1024*1024*1024, "ram size is unrealistic" );
      _gstate.max_ram_size = max_ram_size;
   }

   void system_contract::setparams( const eosio::blockchain_parameters& params ) {
      require_auth( _self );
      (eosio::blockchain_parameters&)(_gstate) = params;
      check( 3 <= _gstate.max_authority_depth, "max_authority_depth should be at least 3" );
      set_blockchain_parameters( params );
   }

   void system_contract::setguaminres(uint32_t cpu)
   {
      require_auth(_self);
      const static uint32_t max_microsec = 10 * 1000 * 1000; // 10 seconds

      eosio_assert( cpu <= max_microsec , "the value of cpu should not more then 10 seconds");
      eosio_guaranteed_min_res _gmr = _guarantee.exists() ? _guarantee.get() : eosio_guaranteed_min_res{};
      eosio_assert( _gmr.cpu < cpu, "can not reduce cpu guarantee");

      _gmr.cpu = cpu;
      _guarantee.set(_gmr, _self);

      set_guaranteed_minimum_resources(0, cpu, 0);
   }

   void system_contract::setpriv( name account, uint8_t ispriv ) {
      require_auth( _self );
      set_privileged( account.value, ispriv );
   }

   void system_contract::setalimits( name account, int64_t cpu ) {
      require_auth( _self );

      user_resources_table userres( _self, account.value );
      auto ritr = userres.find( account.value );
      check( ritr == userres.end(), "only supports unlimited accounts" );

      set_resource_limits_cpu( account.value, cpu );
   }

   void system_contract::rmvproducer( name producer ) {
      require_auth( _self );
      auto prod = _producers.find( producer.value );
      check( prod != _producers.end(), "producer not found" );
      _producers.modify( prod, same_payer, [&](auto& p) {
            p.deactivate();
      });
   }

   void system_contract::updtrevision( uint8_t revision ) {
      require_auth( _self );
      check( _gstate2.revision < 255, "can not increment revision" ); // prevent wrap around
      check( revision == _gstate2.revision + 1, "can only increment revision by one" );
      check( revision <= 1, // set upper bound to greatest revision supported in the code
                    "specified revision is not yet supported by the code" );
      _gstate2.revision = revision;
   }

   /**
    *  Called after a new account is created. This code enforces resource-limits rules
    *  for new accounts as well as new account naming conventions.
    *
    *  Account names containing '.' symbols must have a suffix equal to the name of the creator.
    *  This allows users who buy a premium name (shorter than 12 characters with no dots) to be the only ones
    *  who can create accounts with the creator's name as a suffix.
    *
    */
   void native::newaccount( name              creator,
                            name              newact,
                            ignore<authority> owner,
                            ignore<authority> active ) {

      if( creator != _self ) {
         uint64_t tmp = newact.value >> 4;
         bool has_dot = false;

         for( uint32_t i = 0; i < 12; ++i ) {
           has_dot |= !(tmp & 0x1f);
           tmp >>= 5;
         }
         if( has_dot ) { // or is less than 12 characters
            auto suffix = newact.suffix();
            check( suffix != newact, "short root name must created by eosio authority" );
            check( creator == suffix, "only suffix may create this account" );
         }
      }

      user_resources_table userres( _self, newact.value);
      userres.emplace( newact, [&]( auto& res ) {
        res.owner = newact;
        res.net_weight = asset( 0, system_contract::get_core_symbol() );
        res.cpu_weight = asset( 0, system_contract::get_core_symbol() );
      });

      set_resource_limits_cpu( newact.value, 0 );
   }

   void native::setabi( name acnt, const std::vector<char>& abi ) {
      eosio::multi_index< "abihash"_n, abi_hash >  table(_self, _self.value);
      auto itr = table.find( acnt.value );
      if( itr == table.end() ) {

         table.emplace( acnt, [&]( auto& row ) {
            row.owner= acnt;
            sha256( const_cast<char*>(abi.data()), abi.size(), &row.hash );
         });
      } else {
         table.modify( itr, same_payer, [&]( auto& row ) {
            sha256( const_cast<char*>(abi.data()), abi.size(), &row.hash );
         });
      }
   }

   void native::setcode( name account, uint8_t vmtype, uint8_t vmversion, const std::vector<char>& code ){

   }

   void system_contract::init( unsigned_int version, symbol core ) {
      require_auth( _self );
      check( version.value == 0, "unsupported version for init action" );

      auto system_token_supply = eosio::token::get_supply(token_account, core.code() );
      check( system_token_supply.symbol == core, "specified core symbol does not exist (precision mismatch)" );
      check( system_token_supply.amount > 0, "system token supply must be greater than 0" );

      _gstate2.core_symbol = core;
   }

   void system_contract::buyram( name payer, name receiver, asset quant ){
      check( quant.amount == 0, "buyram action's asset.amount must be zero ");
   }

   void system_contract::buyrambytes( name payer, name receiver, uint32_t bytes ){
      check( bytes == 0, "buyrambytes action's bytes must be zero ");
   }
} /// eosio.system


EOSIO_DISPATCH( eosiosystem::system_contract,
     // native.hpp (newaccount definition is actually in eosio.system.cpp)
     (newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror) // (setabi)
     // eosio.system.cpp
     (init)(setram)(setparams)(setguaminres)(setpriv)(setalimits)(rmvproducer)(updtrevision)(buyram)(buyrambytes)
     // delegate_bandwidth.cpp
     (delegatebw)(undelegatebw)(refund)
     // voting.cpp
     (regproducer)(unregprod)(voteproducer)(regproxy)
     // producer_pay.cpp
     (onblock)(claimrewards)
     //upgrade.cpp
     (setupgrade)
)
