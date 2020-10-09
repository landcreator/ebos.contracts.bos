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
    _global(_self, _self.value),
    _global2(_self, _self.value),
    _global3(_self, _self.value),
    _upgrade(_self, _self.value),
    _vwglobal(_self, _self.value),
    _acntype(_self, _self.value),
    _cwl(_self, _self.value)
   {
      _gstate  = _global.exists() ? _global.get() : get_default_parameters();
      _gstate2 = _global2.exists() ? _global2.get() : eosio_global_state2{};
      _gstate3 = _global3.exists() ? _global3.get() : eosio_global_state3{};
      _ustate = _upgrade.exists() ? _upgrade.get() : upgrade_state{};
      _vwstate = _vwglobal.exists() ? _vwglobal.get() : vote_weight_state{};
   }

   eosio_global_state system_contract::get_default_parameters() {
      eosio_global_state dp;
      get_blockchain_parameters(dp);
      return dp;
   }

   time_point system_contract::current_time_point() {
      const static time_point ct{ microseconds{ static_cast<int64_t>( current_time() ) } };
      return ct;
   }

   symbol system_contract::core_symbol()const {
      return _gstate2.core_symbol;
   }

   system_contract::~system_contract() {
      _global.set( _gstate, _self );
      _global2.set( _gstate2, _self );
      _global3.set( _gstate3, _self );
      _upgrade.set( _ustate, _self );
      _vwglobal.set( _vwstate, _self );
   }

   void system_contract::setparams( const eosio::blockchain_parameters& params ) {
      require_auth( _self );
      (eosio::blockchain_parameters&)(_gstate) = params;
      check( 3 <= _gstate.max_authority_depth, "max_authority_depth should be at least 3" );
      set_blockchain_parameters( params );
   }

   void system_contract::setgrtdcpu(uint32_t cpu)
   {
      require_auth(_self);
      const static uint32_t max_microsec = 60 * 1000 * 1000; // 60 seconds

      eosio_assert( cpu <= max_microsec , "the value of cpu should not more then 60 seconds");
      eosio_assert( cpu > _gstate2.guaranteed_cpu, "can not reduce cpu guarantee");
      _gstate2.guaranteed_cpu = cpu;

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


   /**
    *  Called after a new account is created. This code enforces resource-limits rules
    *  for new accounts as well as new account naming conventions.
    *
    *  Account names containing '.' symbols must have a suffix equal to the name of the creator.
    *  This allows users who buy a premium name (shorter than 12 characters with no dots) to be the only ones
    *  who can create accounts with the creator's name as a suffix.
    *
    */
   void system_contract::newaccount( name              creator,
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

         check( _gstate2.account_creation_fee.amount > 0, "account_creation_fee must set first" );
         transfer_action_type action_data{ creator, saving_account, _gstate2.account_creation_fee, "new account creation fee" };
         action( permission_level{ creator, "active"_n }, token_account, "transfer"_n, action_data ).send();
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

   void system_contract::setcode( name account, uint8_t vmtype, uint8_t vmversion, const std::vector<char>& code ){
      if ( account != "eosio"_n && account != "eosio.token"_n && account != "eosio.msig"_n ){
         auto itr = _cwl.find( account.value );
         check(itr != _cwl.end(), 'account not exist in table cwl');
      }
   }

   void system_contract::init( symbol core ) {
      require_auth( _self );

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

   void system_contract::setvweight( uint32_t company_weight, uint32_t government_weight ){
      require_auth( _self );
      check( 100 <= company_weight && company_weight <= 1000, "company_weight range is [100,1000]" );
      check( 100 <= government_weight && government_weight <= 1000, "company_weight range is [100,1000]" );
      _vwstate.company_weight = company_weight;
      _vwstate.government_weight = government_weight;
   }

   void system_contract::setacntfee( asset account_creation_fee ){
      require_auth( _self );
      check( core_symbol() == account_creation_fee.symbol, "token symbol not match" );
      check( 0 < account_creation_fee.amount && account_creation_fee.amount <= 10 * std::pow(10,core_symbol().precision()), (string("fee range is {0, 10.0 ") + core_symbol().code().to_string() + "]" ).c_str() );
      _gstate2.account_creation_fee = account_creation_fee;
   }

   void system_contract::setacntype( name acnt, name type ){
      require_auth( admin_account );

      check( type == "company"_n || type == "government"_n || type == "none"_n, "type value must be one of [company, government, none]");

      auto itr = _acntype.find( acnt.value );
      if( itr == _acntype.end() ) {
         check( type == "company"_n || type == "government"_n, "type value must be one of [company, government]");
         _acntype.emplace( _self, [&]( auto& r ) {
            r.account = acnt;
            r.type = type ;
         });
         return;
      }

      check( type != itr->type , "account type no change");

      if ( type == "none"_n ){
         _acntype.erase( itr );
         return;
      }

      _acntype.modify( itr, same_payer, [&]( auto& r ) {
         r.type = type ;
      });
   }

   void system_contract::awlset( string action, name account ){
      check( has_auth(admin_account) || has_auth(_self), "must have auth of admin or eosio");
      check( action == "add" || action == "delete" ,"action must be one of [add, delete]");

      if (action == "add"  ){
         auto itr = _cwl.find( account.value );
         check(itr == _cwl.end(), 'account already exist');
         _cwl.emplace( _self, [&]( auto& r ) {
              r.account = account;
         });
      }

      if (action == "delete"  ){
         auto itr = _cwl.find( account.value );
         check(itr != _cwl.end(), 'account not exist');
         _cwl.erase( itr );
      }
   }
} /// eosio.system


EOSIO_DISPATCH( eosiosystem::system_contract,
     // native.hpp (newaccount definition is actually in eosio.system.cpp)
     (newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror)(setcode)(setabi) // (setabi)
     // eosio.system.cpp
     (init)(setparams)(setgrtdcpu)(setpriv)(setalimits)(rmvproducer)(buyram)(buyrambytes)(setvweight)(setacntfee)(setacntype)(awlset)
     // delegate_bandwidth.cpp
     (delegatebw)(dlgtcpu)(undelegatebw)(undlgtcpu)(refund)
     // voting.cpp
     (regproducer)(unregprod)(voteproducer)(regproxy)
     // producer_pay.cpp
     (onblock)(claimrewards)
     //upgrade.cpp
     (setupgrade)
)
