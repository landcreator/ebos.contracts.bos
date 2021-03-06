/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio.system/eosio.system.hpp>

#include <eosiolib/eosio.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/privileged.h>
#include <eosiolib/transaction.hpp>

#include <eosio.token/eosio.token.hpp>

#include <cmath>
#include <map>

namespace eosiosystem {
   using eosio::asset;
   using eosio::indexed_by;
   using eosio::const_mem_fun;
   using eosio::permission_level;
   using eosio::time_point_sec;
   using std::map;
   using std::pair;

   static constexpr uint32_t refund_delay_sec = 3*24*3600;

   struct [[eosio::table, eosio::contract("eosio.system")]] user_resources {
      name          owner;
      asset         net_weight;
      asset         cpu_weight;
      int64_t       ram_bytes = 0;

      bool is_empty()const { return cpu_weight.amount == 0 ; }
      uint64_t primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( user_resources, (owner)(net_weight)(cpu_weight)(ram_bytes) )
   };


   /**
    *  Every user 'from' has a scope/table that uses every receipient 'to' as the primary key.
    */
   struct [[eosio::table, eosio::contract("eosio.system")]] delegated_bandwidth {
      name          from;
      name          to;
      asset         net_weight;
      asset         cpu_weight;

      bool is_empty()const { return cpu_weight.amount == 0; }
      uint64_t  primary_key()const { return to.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( delegated_bandwidth, (from)(to)(net_weight)(cpu_weight) )

   };

   struct [[eosio::table, eosio::contract("eosio.system")]] refund_request {
      name            owner;
      time_point_sec  request_time;
      eosio::asset    net_amount;
      eosio::asset    cpu_amount;

      bool is_empty()const { return cpu_amount.amount == 0; }
      uint64_t  primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( refund_request, (owner)(request_time)(net_amount)(cpu_amount) )
   };

   /**
    *  These tables are designed to be constructed in the scope of the relevant user, this
    *  facilitates simpler API for per-user queries
    */
   typedef eosio::multi_index< "userres"_n, user_resources >      user_resources_table;
   typedef eosio::multi_index< "delband"_n, delegated_bandwidth > del_bandwidth_table;
   typedef eosio::multi_index< "refunds"_n, refund_request >      refunds_table;

   void system_contract::changebw( name from, name receiver, const asset stake_cpu_delta, bool transfer )
   {
      require_auth( from );
      check( stake_cpu_delta.amount != 0, "should stake non-zero stake_cpu_delta.amount" );

      name source_stake_from = from;
      if ( transfer ) {
         from = receiver;
      }

      // update stake delegated from "from" to "receiver"
      {
         del_bandwidth_table     del_tbl( _self, from.value );
         auto itr = del_tbl.find( receiver.value );
         if( itr == del_tbl.end() ) {
            itr = del_tbl.emplace( from, [&]( auto& dbo ){
                  dbo.from          = from;
                  dbo.to            = receiver;
                  dbo.cpu_weight    = stake_cpu_delta;
            });
         } else {
            del_tbl.modify( itr, same_payer, [&]( auto& dbo ){
                  dbo.cpu_weight    += stake_cpu_delta;
            });
         }

         check( 0 <= itr->cpu_weight.amount, "insufficient staked cpu bandwidth" );
         if ( itr->is_empty() ) {
            del_tbl.erase( itr );
         }
      } // itr can be invalid, should go out of scope

      // update totals of "receiver"
      {
         user_resources_table   totals_tbl( _self, receiver.value );
         auto tot_itr = totals_tbl.find( receiver.value );
         if( tot_itr ==  totals_tbl.end() ) {
            tot_itr = totals_tbl.emplace( from, [&]( auto& tot ) {
                  tot.owner      = receiver;
                  tot.cpu_weight = stake_cpu_delta;
            });
         } else {
            totals_tbl.modify( tot_itr, from == receiver ? from : same_payer, [&]( auto& tot ) {
                  tot.cpu_weight += stake_cpu_delta;
            });
         }

         check( 0 <= tot_itr->cpu_weight.amount, "insufficient staked total cpu bandwidth" );
         set_resource_limits_cpu( receiver.value, tot_itr->cpu_weight.amount );
         if ( tot_itr->is_empty() ) {
            totals_tbl.erase( tot_itr );
         }
      } // tot_itr can be invalid, should go out of scope

      // create refund or update from existing refund
      if ( stake_account != source_stake_from ) { //for eosio both transfer and refund make no sense
         refunds_table refunds_tbl( _self, from.value );
         auto req = refunds_tbl.find( from.value );

         //create/update/delete refund
         auto cpu_balance = stake_cpu_delta;
         bool need_deferred_trx = false;

         // redundant assertion also at start of changebw to protect against misuse of changebw
         bool is_undelegating = cpu_balance.amount < 0;
         bool is_delegating_to_self = (!transfer && from == receiver);

         if( is_delegating_to_self || is_undelegating ) {
            if ( req != refunds_tbl.end() ) { //need to update refund
               refunds_tbl.modify( req, same_payer, [&]( refund_request& r ) {
                  if ( cpu_balance.amount < 0 ) {
                     r.request_time = current_time_point();
                  }

                  r.cpu_amount -= cpu_balance;

                  if ( r.cpu_amount.amount < 0 ){
                     r.cpu_amount.amount = 0;
                     cpu_balance = -r.cpu_amount;
                  } else {
                     cpu_balance.amount = 0;
                  }
               });

               check( 0 <= req->cpu_amount.amount, "negative cpu refund amount" ); //should never happen

               if ( req->is_empty() ) {
                  refunds_tbl.erase( req );
                  need_deferred_trx = false;
               } else {
                  need_deferred_trx = true;
               }
            } else if ( cpu_balance.amount < 0 ) { //need to create refund
               refunds_tbl.emplace( from, [&]( refund_request& r ) {
                  r.owner = from;
                  r.cpu_amount = -cpu_balance;
                  cpu_balance.amount = 0;
                  r.request_time = current_time_point();
               });
               need_deferred_trx = true;
            } // else stake increase requested with no existing row in refunds_tbl -> nothing to do with refunds_tbl
         } /// end if is_delegating_to_self || is_undelegating

         if ( need_deferred_trx ) {
            eosio::transaction out;
            out.actions.emplace_back( permission_level{from, active_permission},  _self, "refund"_n,  from );
            out.delay_sec = refund_delay_sec;
            cancel_deferred( from.value );
            out.send( from.value, from, true );
         } else {
            cancel_deferred( from.value );
         }

         auto transfer_amount = cpu_balance;
         if ( 0 < transfer_amount.amount ) {
            INLINE_ACTION_SENDER(eosio::token, transfer)(
               token_account, { {source_stake_from, active_permission} },
               { source_stake_from, stake_account, asset(transfer_amount), std::string("stake bandwidth") }
            );
         }
      }

      update_voting_power( from, stake_cpu_delta );
   }

   void system_contract::update_voting_power( const name& voter, const asset& total_update )
   {
      int64_t old_staked = 0;
      int64_t new_staked = 0;

      auto voter_itr = _voters.find( voter.value );
      if( voter_itr == _voters.end() ) {
         voter_itr = _voters.emplace( voter, [&]( auto& v ) {
            v.owner  = voter;
            v.staked = total_update.amount;
         });
         old_staked = 0;
         new_staked = voter_itr->staked;
      } else {
         old_staked = voter_itr->staked;
         _voters.modify( voter_itr, same_payer, [&]( auto& v ) {
            v.staked += total_update.amount;
         });
         new_staked = voter_itr->staked;
      }

      check( 0 <= voter_itr->staked, "stake for voting cannot be negative" );

      if( voter_itr->producers.size() ) {
          auto itr = _acntype.find( voter.value );
          if( itr != _acntype.end() ){
             update_producers_votes( itr->type , false, voter_itr->producers, old_staked, voter_itr->producers, new_staked);
          }
      }
   }

   void system_contract::delegatebw( name from, name receiver,
                                     asset stake_net_quantity,
                                     asset stake_cpu_quantity, bool transfer ) {
      asset zero_asset( 0, core_symbol() );
      check( stake_net_quantity == zero_asset, "stake_net_quantity must be zero asset" );
      check( stake_cpu_quantity >  zero_asset, "must stake a positive amount" );
      check( stake_cpu_quantity.amount > 0, "must stake a positive amount" );
      check( !transfer || from != receiver, "cannot use transfer flag if delegating to self" );

      changebw( from, receiver, stake_cpu_quantity, transfer );
   }

   void system_contract::dlgtcpu( name from, name receiver, asset stake_cpu_quantity, bool transfer ){
      asset zero_asset( 0, core_symbol() );
      delegatebw( from, receiver, zero_asset, stake_cpu_quantity, transfer );
   }

   void system_contract::undelegatebw( name from, name receiver,
                                       asset unstake_net_quantity,
                                       asset unstake_cpu_quantity ) {
      asset zero_asset( 0, core_symbol() );
      check( unstake_net_quantity == zero_asset, "unstake_net_quantity must be zero asset" );
      check( unstake_cpu_quantity >  zero_asset, "must unstake a positive amount" );
      check( unstake_cpu_quantity.amount + unstake_net_quantity.amount > 0, "must unstake a positive amount" );

      changebw( from, receiver, -unstake_cpu_quantity, false);
   }

   void system_contract::undlgtcpu( name from, name receiver, asset unstake_cpu_quantity ){
      asset zero_asset( 0, core_symbol() );
      undelegatebw( from, receiver, zero_asset, unstake_cpu_quantity );
   }

   void system_contract::refund( const name owner ) {
      require_auth( owner );

      refunds_table refunds_tbl( _self, owner.value );
      auto req = refunds_tbl.find( owner.value );
      check( req != refunds_tbl.end(), "refund request not found" );
      check( req->request_time + seconds(refund_delay_sec) <= current_time_point(), "refund is not available yet" );

      INLINE_ACTION_SENDER(eosio::token, transfer)(
         token_account, { {stake_account, active_permission}, {req->owner, active_permission} },
         { stake_account, req->owner, req->cpu_amount, std::string("unstake") }
      );

      refunds_tbl.erase( req );
   }


} //namespace eosiosystem
