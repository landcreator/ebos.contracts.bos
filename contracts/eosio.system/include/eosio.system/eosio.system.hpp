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

      symbol            core_symbol;
      double            total_producer_votepay_share = 0;
      uint8_t           revision = 0; ///< used to track version updates in the future.

      EOSLIB_SERIALIZE( eosio_global_state2, (core_symbol)(total_producer_votepay_share)(revision) )
   };

   struct [[eosio::table("global3"), eosio::contract("eosio.system")]] eosio_global_state3 {
      eosio_global_state3() { }
      time_point        last_vpay_state_update;
      double            total_vpay_share_change_rate = 0;

      EOSLIB_SERIALIZE( eosio_global_state3, (last_vpay_state_update)(total_vpay_share_change_rate) )
   };

   struct [[eosio::table, eosio::contract("eosio.system")]] producer_info {
      name                  owner;
      double                total_votes = 0;
      eosio::public_key     producer_key; /// a packed public key object
      bool                  is_active = true;
      std::string           url;
      uint32_t              unpaid_blocks = 0;
      time_point            last_claim_time;
      uint16_t              location = 0;

      uint64_t primary_key()const { return owner.value;                             }
      double   by_votes()const    { return is_active ? -total_votes : total_votes;  }
      bool     active()const      { return is_active;                               }
      void     deactivate()       { producer_key = public_key(); is_active = false; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( producer_info, (owner)(total_votes)(producer_key)(is_active)(url)
                        (unpaid_blocks)(last_claim_time)(location) )
   };

   struct [[eosio::table, eosio::contract("eosio.system")]] producer_info2 {
      name            owner;
      double          votepay_share = 0;
      time_point      last_votepay_share_update;

      uint64_t primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( producer_info2, (owner)(votepay_share)(last_votepay_share_update) )
   };

   struct [[eosio::table, eosio::contract("eosio.system")]] voter_info {
      name                owner;     /// the voter
      name                proxy;     /// the proxy set by the voter, if any
      std::vector<name>   producers; /// the producers approved by this voter if no proxy set
      int64_t             staked = 0;

      /**
       *  Every time a vote is cast we must first "undo" the last vote weight, before casting the
       *  new vote weight.  Vote weight is calculated as:
       *
       *  stated.amount * 2 ^ ( weeks_since_launch/weeks_per_year)
       */
      double              last_vote_weight = 0; /// the vote weight cast the last time the vote was updated

      /**
       * Total vote weight delegated to this voter.
       */
      double              proxied_vote_weight= 0; /// the total vote weight delegated to this voter as a proxy
      bool                is_proxy = 0; /// whether the voter is a proxy for others


      uint32_t            flags1 = 0;
      uint32_t            reserved2 = 0;
      eosio::asset        reserved3;

      uint64_t primary_key()const { return owner.value; }

      enum class flags1_fields : uint32_t {
         ram_managed = 1,
         net_managed = 2,
         cpu_managed = 4
      };

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( voter_info, (owner)(proxy)(producers)(staked)(last_vote_weight)(proxied_vote_weight)(is_proxy)(flags1)(reserved2)(reserved3) )
   };

   struct [[eosio::table("guaranminres"), eosio::contract("eosio.system")]] eosio_guaranteed_min_res{
      eosio_guaranteed_min_res(){}

      uint32_t ram = 0;  ///  guaranteed minimum ram  in kb for every account.
      uint32_t cpu = 0;  ///  guaranteed minimum cpu  in bos for every account.
      uint32_t net = 0;  ///  guaranteed minimum net  in bos for every account.

      EOSLIB_SERIALIZE( eosio_guaranteed_min_res, (ram)(cpu)(net) )
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
   typedef eosio::multi_index< "producers2"_n, producer_info2 > producers_table2;

   typedef eosio::multi_index< "voters"_n, voter_info >  voters_table;
   typedef eosio::singleton< "guaranminres"_n, eosio_guaranteed_min_res > guaranteed_min_res_singleton;
   typedef eosio::singleton< "upgrade"_n, upgrade_state > upgrade_singleton;

   static constexpr uint32_t     seconds_per_day = 24 * 3600;

   class [[eosio::contract("eosio.system")]] system_contract : public native {
      private:
         voters_table            _voters;
         producers_table         _producers;
         producers_table2        _producers2;
         global_state_singleton  _global;
         global_state2_singleton _global2;
         global_state3_singleton _global3;
         guaranteed_min_res_singleton  _guarantee;
         eosio_global_state      _gstate;
         eosio_global_state2     _gstate2;
         eosio_global_state3     _gstate3;
         upgrade_singleton       _upgrade;
         upgrade_state           _ustate;

      public:
         static constexpr eosio::name active_permission{"active"_n};
         static constexpr eosio::name token_account{"eosio.token"_n};
         static constexpr eosio::name stake_account{"eosio.stake"_n};
         static constexpr eosio::name bpay_account{"eosio.bpay"_n};
         static constexpr eosio::name vpay_account{"eosio.vpay"_n};
         static constexpr eosio::name names_account{"eosio.names"_n};
         static constexpr eosio::name saving_account{"eosio.saving"_n};
         static constexpr eosio::name null_account{"eosio.null"_n};

         system_contract( name s, name code, datastream<const char*> ds );
         ~system_contract();

         static symbol get_core_symbol(){
            auto _global2 = global_state2_singleton("eosio"_n,"eosio"_n.value);
            check( _global2.exists(), "system contract not initialized");
            return _global2.get().core_symbol;
         }

         [[eosio::action]]
         void init( unsigned_int version, symbol core );

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
         [[eosio::action]]
         void delegatebw( name from, name receiver,
                          asset stake_net_quantity, asset stake_cpu_quantity, bool transfer );

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
         [[eosio::action]]
         void undelegatebw( name from, name receiver,
                            asset unstake_net_quantity, asset unstake_cpu_quantity );

         /**
          *  This action is called after the delegation-period to claim all pending
          *  unstaked tokens belonging to owner
          */
         [[eosio::action]]
         void refund( name owner );

         /// functions defined in voting.cpp
         [[eosio::action]]
         void regproducer( const name producer, const public_key& producer_key, const std::string& url, uint16_t location );

         [[eosio::action]]
         void unregprod( const name producer );

         [[eosio::action]]
         void setram( uint64_t max_ram_size );

         [[eosio::action]]
         void voteproducer( const name voter, const name proxy, const std::vector<name>& producers );

         [[eosio::action]]
         void regproxy( const name proxy, bool isproxy );

         [[eosio::action]]
         void setparams( const eosio::blockchain_parameters& params );

         [[eosio::action]]
         void setguaminres( uint32_t cpu );

         /// functions defined in producer_pay.cpp
         [[eosio::action]]
         void claimrewards( const name owner );

         [[eosio::action]]
         void setpriv( name account, uint8_t is_priv );

         [[eosio::action]]
         void rmvproducer( name producer );

         [[eosio::action]]
         void updtrevision( uint8_t revision );

         struct upgrade_proposal {
             uint32_t    target_block_num;
         };

         /// functions defined in upgrade.cpp
         [[eosio::action]]
         void setupgrade( const upgrade_proposal& up);

         [[eosio::action]]
         void buyram( name payer, name receiver, asset quant );

         [[eosio::action]]
         void buyrambytes( name payer, name receiver, uint32_t bytes );

         using init_action = eosio::action_wrapper<"init"_n, &system_contract::init>;
         using delegatebw_action = eosio::action_wrapper<"delegatebw"_n, &system_contract::delegatebw>;
         using undelegatebw_action = eosio::action_wrapper<"undelegatebw"_n, &system_contract::undelegatebw>;
         using refund_action = eosio::action_wrapper<"refund"_n, &system_contract::refund>;
         using regproducer_action = eosio::action_wrapper<"regproducer"_n, &system_contract::regproducer>;
         using unregprod_action = eosio::action_wrapper<"unregprod"_n, &system_contract::unregprod>;
         using setram_action = eosio::action_wrapper<"setram"_n, &system_contract::setram>;
         using voteproducer_action = eosio::action_wrapper<"voteproducer"_n, &system_contract::voteproducer>;
         using regproxy_action = eosio::action_wrapper<"regproxy"_n, &system_contract::regproxy>;
         using claimrewards_action = eosio::action_wrapper<"claimrewards"_n, &system_contract::claimrewards>;
         using rmvproducer_action = eosio::action_wrapper<"rmvproducer"_n, &system_contract::rmvproducer>;
         using updtrevision_action = eosio::action_wrapper<"updtrevision"_n, &system_contract::updtrevision>;
         using setpriv_action = eosio::action_wrapper<"setpriv"_n, &system_contract::setpriv>;
         using setalimits_action = eosio::action_wrapper<"setalimits"_n, &system_contract::setalimits>;
         using setparams_action = eosio::action_wrapper<"setparams"_n, &system_contract::setparams>;

      private:

         //defined in eosio.system.cpp
         static eosio_global_state  get_default_parameters();
         static time_point current_time_point();
         symbol core_symbol()const;

         //defined in delegate_bandwidth.cpp
         void changebw( name from, name receiver, asset stake_cpu_quantity, bool transfer );
         void update_voting_power( const name& voter, const asset& total_update );

         //defined in voting.hpp
         void update_elected_producers( block_timestamp timestamp );
         void update_votes( const name voter, const name proxy, const std::vector<name>& producers, bool voting );

         // defined in voting.cpp
         void propagate_weight_change( const voter_info& voter );

         double update_producer_votepay_share( const producers_table2::const_iterator& prod_itr,
                                               time_point ct,
                                               double shares_rate, bool reset_to_zero = false );
         double update_total_votepay_share( time_point ct,
                                            double additional_shares_delta = 0.0, double shares_rate_delta = 0.0 );
   };

} /// eosiosystem
