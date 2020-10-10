/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio.system/native.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/privileged.hpp>
#include <eosiolib/singleton.hpp>

#include <string>
#include <deque>
#include <type_traits>
#include <optional>

namespace eosiosystem {

   using eosio::name;
   using eosio::asset;
   using eosio::symbol;
   using eosio::symbol_code;
   using eosio::indexed_by;
   using eosio::const_mem_fun;
   using eosio::block_timestamp;
   using eosio::time_point;
   using eosio::time_point_sec;
   using eosio::microseconds;
   using eosio::datastream;
   using eosio::check;
   using std::string;

   const static name name_company = "company"_n;
   const static name name_government = "government"_n;

   struct transfer_action_type {
      name    from;
      name    to;
      asset   quantity;
      string  memo;

      EOSLIB_SERIALIZE( transfer_action_type, (from)(to)(quantity)(memo) )
   };

   template<typename E, typename F>
   static inline auto has_field( F flags, E field )
   -> std::enable_if_t< std::is_integral_v<F> && std::is_unsigned_v<F> &&
                        std::is_enum_v<E> && std::is_same_v< F, std::underlying_type_t<E> >, bool>
   {
      return ( (flags & static_cast<F>(field)) != 0 );
   }

   template<typename E, typename F>
   static inline auto set_field( F flags, E field, bool value = true )
   -> std::enable_if_t< std::is_integral_v<F> && std::is_unsigned_v<F> &&
                        std::is_enum_v<E> && std::is_same_v< F, std::underlying_type_t<E> >, F >
   {
      if( value )
         return ( flags | static_cast<F>(field) );
      else
         return ( flags & ~static_cast<F>(field) );
   }

   struct [[eosio::table("voteweight"), eosio::contract("eosio.system")]] vote_weight_state {
      vote_weight_state() {}
      uint32_t  company_weight = 100;        /// base number is 100
      uint32_t  government_weight = 100;     /// base number is 100

      EOSLIB_SERIALIZE( vote_weight_state, (company_weight)(government_weight) )
   };
   typedef eosio::singleton< "voteweight"_n, vote_weight_state >   vote_weight_singleton;

   struct [[eosio::table("acntype"), eosio::contract("eosio.system")]] ebos_account_type {
      ebos_account_type() { }
      name   account;
      name   type;  /// must be "company" or "government"

      uint64_t primary_key()const { return account.value; }
      EOSLIB_SERIALIZE( ebos_account_type, (account)(type) )
   };
   typedef eosio::multi_index< "acntype"_n, ebos_account_type >  account_type_table;

   struct [[eosio::table("cwl"), eosio::contract("eosio.system")]] ebos_contract_white_list {
      ebos_contract_white_list() { }
      name   account;

      uint64_t primary_key()const { return account.value; }
      EOSLIB_SERIALIZE( ebos_contract_white_list, (account) )
   };
   typedef eosio::multi_index< "cwl"_n, ebos_contract_white_list >  cwl_table;

    /**
    * eosio.system contract defines the structures and actions needed for blockchain's core functionality.
    * - There are three types of accounts, ordinary user accounts, corporate accounts, and government accounts.
    * - Users can stake tokens for CPU, and then corporate and government accounts could vote for producers or delegate their vote to a proxy.
    * - Producers register in order to be voted for, and can claim per-block and per-vote rewards.
    */

   struct [[eosio::table("global"), eosio::contract("eosio.system")]] eosio_global_state : eosio::blockchain_parameters {
      uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; }

      uint64_t             max_ram_size = 64ll*1024 * 1024 * 1024;
      uint64_t             total_ram_bytes_reserved = 0;
      int64_t              total_ram_stake = 0;

      block_timestamp      last_producer_schedule_update;
      time_point           last_pervote_bucket_fill;
      int64_t              pervote_bucket = 0;
      int64_t              perblock_bucket = 0;
      uint32_t             total_unpaid_blocks = 0; /// all blocks which have been produced but not paid
      int64_t              total_activated_stake = 0;
      time_point           thresh_activated_stake_time;
      uint16_t             last_producer_schedule_size = 0;
      double               total_producer_vote_weight = 0; /// the sum of all producer votes
      block_timestamp      last_name_close;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE_DERIVED( eosio_global_state, eosio::blockchain_parameters,
                                (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
                                (last_producer_schedule_update)(last_pervote_bucket_fill)
                                (pervote_bucket)(perblock_bucket)(total_unpaid_blocks)(total_activated_stake)(thresh_activated_stake_time)
                                (last_producer_schedule_size)(total_producer_vote_weight)(last_name_close) )
   };

   /**
    * Defines new global state parameters added after version 1.0
    */
   struct [[eosio::table("global2"), eosio::contract("eosio.system")]] eosio_global_state2 {
      eosio_global_state2(){}

      symbol   core_symbol;
      asset    account_creation_fee;
      uint32_t guaranteed_cpu = 2 * 1000 * 1000; // 3 seconds

      EOSLIB_SERIALIZE( eosio_global_state2, (core_symbol)(account_creation_fee)(guaranteed_cpu) )
   };

   struct [[eosio::table("global3"), eosio::contract("eosio.system")]] eosio_global_state3 {
      eosio_global_state3() { }
      time_point        last_vpay_state_update;
      double            total_vpay_share_change_rate = 0;

      EOSLIB_SERIALIZE( eosio_global_state3, (last_vpay_state_update)(total_vpay_share_change_rate) )
   };

   struct [[eosio::table, eosio::contract("eosio.system")]] producer_info {
      name                  owner;
      double                total_vote_weight = 0;
      int64_t               company_votes = 0;
      int64_t               government_votes = 0;
      int64_t               normal_votes = 0;

      eosio::public_key     producer_key; /// a packed public key object
      bool                  is_active = true;
      std::string           url;
      uint32_t              unpaid_blocks = 0;
      time_point            last_claim_time;
      uint16_t              location = 0;

      uint64_t primary_key()const { return owner.value;                             }
      double   by_votes()const    { return is_active ? -total_vote_weight : total_vote_weight;  }
      bool     active()const      { return is_active;                               }
      void     deactivate()       { producer_key = public_key(); is_active = false; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( producer_info, (owner)(total_vote_weight)(company_votes)(government_votes)(normal_votes)(producer_key)(is_active)(url)
                        (unpaid_blocks)(last_claim_time)(location) )
   };

   struct [[eosio::table, eosio::contract("eosio.system")]] voter_info {
      name                owner;     /// the voter
      std::vector<name>   producers; /// the producers approved by this voter
      int64_t             staked = 0;

      uint64_t primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( voter_info, (owner)(producers)(staked) )
   };

   struct [[eosio::table("upgrade"), eosio::contract("eosio.system")]] upgrade_state  {
      uint32_t     target_block_num;

      EOSLIB_SERIALIZE( upgrade_state, (target_block_num) )
   };
   typedef eosio::singleton< "global"_n, eosio_global_state >   global_state_singleton;
   typedef eosio::singleton< "global2"_n, eosio_global_state2 > global_state2_singleton;
   typedef eosio::singleton< "global3"_n, eosio_global_state3 > global_state3_singleton;

   typedef eosio::multi_index< "producers"_n, producer_info,
                               indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes>  >
                               > producers_table;

   typedef eosio::multi_index< "voters"_n, voter_info >  voters_table;
   typedef eosio::singleton< "upgrade"_n, upgrade_state > upgrade_singleton;

   static constexpr uint32_t     seconds_per_day = 24 * 3600;

   class [[eosio::contract("eosio.system")]] system_contract : public native {
      private:
         voters_table            _voters;
         producers_table         _producers;
         global_state_singleton  _global;
         global_state2_singleton _global2;
         global_state3_singleton _global3;
         eosio_global_state      _gstate;
         eosio_global_state2     _gstate2;
         eosio_global_state3     _gstate3;
         upgrade_singleton       _upgrade;
         upgrade_state           _ustate;
         vote_weight_singleton   _vwglobal;
         vote_weight_state       _vwstate;
         account_type_table      _acntype;
         cwl_table               _cwl;

      public:
         static constexpr eosio::name active_permission{"active"_n};
         static constexpr eosio::name token_account{"eosio.token"_n};
         static constexpr eosio::name stake_account{"eosio.stake"_n};
         static constexpr eosio::name saving_account{"eosio.saving"_n};
         static constexpr eosio::name admin_account{"dyadmin"_n};

         system_contract( name s, name code, datastream<const char*> ds );
         ~system_contract();

         static symbol get_core_symbol(){
            auto _global2 = global_state2_singleton("eosio"_n,"eosio"_n.value);
            check( _global2.exists(), "system contract not initialized");
            return _global2.get().core_symbol;
         }

         [[eosio::action]]
         void init( symbol core );

         [[eosio::action]]
         void onblock( ignore<block_header> header );

         [[eosio::action]]
         void setalimits( name account, int64_t cpu_weight );

         /// functions defined in delegate_bandwidth.cpp
         /**
          *  Stakes SYS from the balance of 'from' for the benfit of 'receiver'.
          *  If transfer == true, then 'receiver' can unstake to their account
          *  Else 'from' can unstake at any time.
          */
         [[eosio::action]] /// exist and unchanged for compatibility of eosio community related software apis
         void delegatebw( name from, name receiver,
                          asset stake_net_quantity, asset stake_cpu_quantity, bool transfer );

         [[eosio::action]]
         void dlgtcpu( name from, name receiver, asset stake_cpu_quantity, bool transfer );

      /**
          *  Decreases the total tokens delegated by from to receiver and/or
          *  frees the memory associated with the delegation if there is nothing
          *  left to delegate.
          *
          *  This will cause an immediate reduction in net/cpu bandwidth of the
          *  receiver.
          *
          *  A transaction is scheduled to send the tokens back to 'from' after
          *  the staking period has passed. If existing transaction is scheduled, it
          *  will be canceled and a new transaction issued that has the combined
          *  undelegated amount.
          *
          *  The 'from' account loses voting power as a result of this call and
          *  all producer tallies are updated.
          */
         [[eosio::action]] /// exist and unchanged for compatibility of eosio community related software apis
         void undelegatebw( name from, name receiver,
                            asset unstake_net_quantity, asset unstake_cpu_quantity );
         [[eosio::action]]
         void undlgtcpu( name from, name receiver, asset unstake_cpu_quantity );

         /**
          *  This action is called after the delegation-period to claim all pending
          *  unstaked tokens belonging to owner
          */
         [[eosio::action]]
         void refund( name owner );

         /// functions defined in voting.cpp
         [[eosio::action]] /// unchanged for compatibility of eosio community related software apis
         void regproducer( const name producer, const public_key& producer_key, const std::string& url, uint16_t location );

         [[eosio::action]]
         void unregprod( const name producer );

         [[eosio::action]]
         void voteproducer( const name voter, const name proxy, const std::vector<name>& producers );

         [[eosio::action]]
         void setparams( const eosio::blockchain_parameters& params );

         [[eosio::action]]
         void setgrtdcpu( uint32_t cpu );

         /// functions defined in producer_pay.cpp
         [[eosio::action]]
         void claimrewards( const name owner );

         [[eosio::action]]
         void setpriv( name account, uint8_t is_priv );

         [[eosio::action]]
         void rmvproducer( name producer );

         struct upgrade_proposal {
             uint32_t    target_block_num;
         };

         /// functions defined in upgrade.cpp
         [[eosio::action]]
         void setupgrade( const upgrade_proposal& up);

         [[eosio::action]] /// exist and unchanged for compatibility of eosio community related software apis
         void buyram( name payer, name receiver, asset quant );

         [[eosio::action]] /// exist and unchanged for compatibility of eosio community related software apis
         void buyrambytes( name payer, name receiver, uint32_t bytes );

         [[eosio::action]]
         void setvweight( uint32_t company_weight, uint32_t government_weight );

         [[eosio::action]]
         void setacntfee( asset account_creation_fee );

         [[eosio::action]]
         void awlset( string action, name account );

[[eosio::action]]
    void setcode( name account, uint8_t vmtype, uint8_t vmversion, const std::vector<char>& code );


         [[eosio::action]]
         void setacntype( name account, name type );

         [[eosio::action]]
         void newaccount( name             creator,
                          name             newact,
                          ignore<authority> owner,
                          ignore<authority> active);


   private:
         //defined in eosio.system.cpp
         static eosio_global_state  get_default_parameters();
         static time_point current_time_point();
         symbol core_symbol()const;

         //defined in delegate_bandwidth.cpp
         void changebw( name from, name receiver, asset stake_cpu_quantity, bool transfer );
         void update_voting_power( const name& voter, const asset& total_update );

         //defined in voting.cpp
         void update_elected_producers( block_timestamp timestamp );
         void update_producers_votes( name type, bool voting, const std::vector<name>& old_producers, int64_t old_staked,
                                      const std::vector<name>& new_producers, int64_t new_staked );
   };

} /// eosiosystem
