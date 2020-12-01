/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio.system/eosio.system.hpp>

#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/privileged.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.hpp>
#include <eosio.token/eosio.token.hpp>

#include <algorithm>
#include <cmath>

namespace eosiosystem {
   using eosio::indexed_by;
   using eosio::const_mem_fun;
   using eosio::singleton;
   using eosio::transaction;

   void system_contract::regproducer( const name producer, const eosio::public_key& producer_key, const std::string& url, uint16_t location ) {
      check( url.size() < 512, "url too long" );
      check( producer_key != eosio::public_key(), "public key should not be the default value" );
      require_auth( producer );

      auto prod = _producers.find( producer.value );
      const auto ct = current_time_point();

      if ( prod != _producers.end() ) {
         _producers.modify( prod, producer, [&]( producer_info& info ){
            info.producer_key = producer_key;
            info.is_active    = true;
            info.url          = url;
            info.location     = location;
            if ( info.last_claim_time == time_point() )
               info.last_claim_time = ct;
         });
      } else {
         _producers.emplace( producer, [&]( producer_info& info ){
            info.owner           = producer;
            info.total_vote_weight = 0;
            info.producer_key    = producer_key;
            info.is_active       = true;
            info.url             = url;
            info.location        = location;
            info.last_claim_time = ct;
         });
      }

   }

   void system_contract::unregprod( const name producer ) {
      require_auth( producer );

      const auto& prod = _producers.get( producer.value, "producer not found" );
      _producers.modify( prod, same_payer, [&]( producer_info& info ){
         info.deactivate();
      });
   }

   void system_contract::update_elected_producers( block_timestamp block_time ) {
      _gstate.last_producer_schedule_update = block_time;

      auto idx = _producers.get_index<"prototalvote"_n>();

      std::vector< eosio::producer_key > top_producers;
      top_producers.reserve(21);

      for ( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && 0 < it->total_vote_weight && it->active(); ++it ) {
         top_producers.emplace_back( eosio::producer_key{it->owner, it->producer_key} );
      }

      if ( top_producers.empty() || top_producers.size() < _gstate.last_producer_schedule_size ) {
         return;
      }

      std::vector<eosio::producer_key> producers;

      producers.reserve(top_producers.size());
      for( const auto& item : top_producers )
         producers.push_back(item);

      auto packed_schedule = pack(producers);

      if( set_proposed_producers( packed_schedule.data(),  packed_schedule.size() ) >= 0 ) {
         _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>( top_producers.size() );
      }
   }

   void system_contract::voteproducer( const name voter_name, const name , const std::vector<name>& producers ) {
      require_auth( voter_name );
      check( producers.size() <= 30, "attempt to vote for too many producers" );
      for( size_t i = 1; i < producers.size(); ++i ) {
         check( producers[i-1] < producers[i], "producer votes must be unique and sorted" );
      }

      auto voter_itr = _voters.find( voter_name.value );
      check( voter_itr != _voters.end(), "user must stake before they can vote" );

      auto itr = _acntype.find( voter_name.value );
      check( itr != _acntype.end(), "user must registered as company or government");

      auto old_producers = voter_itr->producers;
      auto old_staked    = voter_itr->staked;
      auto new_producers = producers;
      auto new_staked    = voter_itr->staked;
      auto a_type        = itr->type;

      _voters.modify( voter_itr, same_payer, [&]( auto& v ) {
         v.producers = new_producers;
      });

      update_producers_votes( a_type, true, old_producers, old_staked, new_producers, new_staked );
   }

   void system_contract::update_producers_votes( name a_type, bool voting,
                                                 const std::vector<name>& old_producers, int64_t old_staked,
                                                 const std::vector<name>& new_producers, int64_t new_staked ) {
      if( old_staked != 0){
          for( const auto& p : old_producers  ) {
             auto pitr = _producers.find( p.value );
             check( !voting || pitr->active(), "producer is not currently registered" );
             if ( a_type == name_company ){
                _producers.modify( pitr, same_payer, [&]( auto& p ) {
                   p.company_votes -= old_staked;
                });
             } else {
               _producers.modify( pitr, same_payer, [&]( auto& p ) {
                   p.government_votes -= old_staked;
               });
             }
            _producers.modify( pitr, same_payer, [&]( auto& p ) {
               p.total_vote_weight = p.government_votes * _vwstate.government_weight + p.company_votes * _vwstate.company_weight;
            });
          }
      }

      if ( new_staked != 0 ){
          for( const auto& p : new_producers  ) {
               auto pitr = _producers.find( p.value );
               check( !voting || pitr->active(), "producer is not currently registered" );
               if ( a_type == name_company ){
                  _producers.modify( pitr, same_payer, [&]( auto& p ) {
                     p.company_votes += new_staked;
                  });
               } else {
                 _producers.modify( pitr, same_payer, [&]( auto& p ) {
                     p.government_votes += new_staked;
                 });
               }
               _producers.modify( pitr, same_payer, [&]( auto& p ) {
                  p.total_vote_weight = p.government_votes * _vwstate.government_weight + p.company_votes * _vwstate.company_weight;
               });
          }
      }
   }
} /// namespace eosiosystem
