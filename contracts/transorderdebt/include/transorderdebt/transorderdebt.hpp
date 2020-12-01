#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>

using namespace eosio;

namespace eosio{
  class [[eosio::contract("transorderdebt")]] transorderdebt : public contract{
    public:
      using contract::contract;

      [[eosio::action]]
      void transupsert(checksum256 trans_id, name from, name to, asset quantity, std::string memo, asset fee);

      [[eosio::action]]
      void transerase(checksum256 trans_id);

      [[eosio::action]]
      void orderupsert(uint128_t order_id, name account, std::string logistics, std::string goods_info, name merchant);

      [[eosio::action]]
      void ordererase(uint128_t order_id);

      [[eosio::action]]
      void debtupsert(uint128_t debt_id, name debtor, name creditor, asset quantity, asset fee, std::map<std::string, std::string> profile);

      [[eosio::action]]
      void debterase(uint128_t debt_id);



      using trans_upsert_action = eosio::action_wrapper<"transupsert"_n, &transorderdebt::transupsert>;

      using trans_erase_aciton = eosio::action_wrapper<"transerase"_n, &transorderdebt::transerase>;

      using order_upsert_action = eosio::action_wrapper<"orderupsert"_n, &transorderdebt::orderupsert>;

      using order_erase_action = eosio::action_wrapper<"ordererase"_n, &transorderdebt::ordererase>;

      using debt_upsert_action = eosio::action_wrapper<"debtupsert"_n, &transorderdebt::debtupsert>;

      using debt_erase_aciton = eosio::action_wrapper<"debterase"_n, &transorderdebt::debterase>;

    private:

      struct [[eosio::table]] transrecord{
        uint64_t pkey;
        checksum256 trans_id;
        name from;
        name to;
        asset quantity;
        std::string memo;
        asset fee;
        block_timestamp timestamp;

        uint64_t primary_key() const { return pkey; }
        checksum256 get_secondary_1() const { return trans_id; }
      };

      using transrecord_index = eosio::multi_index<"transrecords"_n, transrecord, indexed_by<"bytransid"_n, const_mem_fun<transrecord,
      checksum256, &transrecord::get_secondary_1>>>;

      struct [[eosio::table]] order{
        uint64_t pkey;
        uint128_t order_id;
        name account;
        std::string logistics;
        std::string goods_info;
        name merchant;
        block_timestamp timestamp;

        uint64_t primary_key() const{ return pkey; }
        uint128_t get_secondary_1() const { return order_id; }
      };

      using order_index = eosio::multi_index<"orders"_n, order, indexed_by<"byorderid"_n, const_mem_fun<order,
      uint128_t, &order::get_secondary_1>>>;

      struct [[eosio::table]] debt{
        uint64_t pkey;
        uint128_t debt_id;
        name debtor;
        name creditor;
        asset quantity;
        asset fee;
        std::map<std::string, std::string> profile;
        block_timestamp timestamp;

        uint64_t primary_key() const { return pkey; }
        uint128_t get_secondary_1() const { return debt_id; }
      };

      using debt_index = eosio::multi_index<"debts"_n, debt, indexed_by<"bydebtid"_n, const_mem_fun<debt,
      uint128_t, &debt::get_secondary_1>>>;
  };
};