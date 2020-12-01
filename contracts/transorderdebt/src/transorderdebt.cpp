#include <transorderdebt/transorderdebt.hpp>

namespace eosio{
  void transorderdebt::transupsert(checksum256 trans_id, name from, name to, asset quantity, std::string memo, asset fee){
    require_auth(get_self());

    check( from != to, "cannot transfer to self" );
    check( is_account( from ), "from account does not exist");
    check( is_account( to ), "to account does not exist");

    check( quantity.is_valid(), "invalid quantity" );
    check( fee.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( fee.amount >= 0, "must transfer positive quantity" );
    check( quantity.symbol == fee.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    transrecord_index transrecords(get_self(), get_self().value);

    auto trans_id_index = transrecords.get_index<name("bytransid")>();

    auto iterator = trans_id_index.find(trans_id);

    if( iterator == trans_id_index.end()){
      transrecords.emplace(get_self(), [&]( auto& row){
        row.pkey = transrecords.available_primary_key();
        row.trans_id = trans_id;
        row.from = from;
        row.to = to;
        row.quantity = quantity;
        row.memo = memo;
        row.fee = fee;
        row.timestamp = current_block_time();
      });
    }
    else{
      transrecords.modify(*iterator, get_self(), [&](auto& row){
        row.trans_id = trans_id;
        row.from = from;
        row.to = to;
        row.quantity = quantity;
        row.memo = memo;
        row.fee = fee;
        row.timestamp = current_block_time();
      });
    }
  }


  void transorderdebt::transerase(checksum256 trans_id){
    require_auth(get_self());

    transrecord_index transrecords(get_self(), get_self().value);

    auto trans_id_index = transrecords.get_index<name("bytransid")>();

    auto iterator = trans_id_index.find(trans_id);

    check(iterator != trans_id_index.end(), "Transrecord does not exist");

    trans_id_index.erase(iterator);
  }


 	void transorderdebt::orderupsert(uint128_t order_id, name account, std::string logistics, std::string goods_info, name merchant){

 		require_auth( get_self() );

 		order_index orders(get_self(), get_self().value);

 		auto order_id_index = orders.get_index<name("byorderid")>();

 		auto iterator = order_id_index.find(order_id);

 		if( iterator == order_id_index.end() )
	    {
	    	orders.emplace(get_self(), [&]( auto& row ) {
	    		row.pkey = orders.available_primary_key();
	    		row.order_id = order_id;
        	row.account = account;
        	row.logistics = logistics;
        	row.goods_info = goods_info;
        	row.merchant = merchant;
        	row.timestamp = current_block_time();
	      });
	    }
	    else {
	      orders.modify(*iterator, get_self(), [&]( auto& row ) {
	        row.order_id = order_id;
	        row.account = account;
	        row.logistics = logistics;
	        row.goods_info = goods_info;
	        row.merchant = merchant;
	        row.timestamp = current_block_time();
	      });
	    }
 	}

 	void transorderdebt::ordererase(uint128_t order_id){

 		require_auth( get_self() );

 		order_index orders(get_self(), get_self().value);

 		auto order_id_index = orders.get_index<name("byorderid")>();

 		auto iterator = order_id_index.find(order_id);

 		check(iterator != order_id_index.end(), "Order does not exist");

 		order_id_index.erase(iterator);
 	}


  void transorderdebt::debtupsert(uint128_t debt_id, name debtor, name creditor, asset quantity, asset fee, std::map<std::string, std::string> profile){
    require_auth(get_self());

    check( debtor != creditor, "debtor and creditor cannot be same one" );
    check( is_account( debtor ), "debtor account does not exist");
    check( is_account( creditor ), "creditor account does not exist");

    check( quantity.is_valid(), "invalid quantity" );
    check( fee.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( fee.amount >= 0, "must transfer positive quantity" );
    check( quantity.symbol == fee.symbol, "symbol precision mismatch" );

    debt_index debts(get_self(), get_self().value);

    auto debt_id_index = debts.get_index<name("bydebtid")>();

    auto iterator = debt_id_index.find(debt_id);

    if( iterator == debt_id_index.end()){
      debts.emplace(get_self(), [&]( auto& row){
        row.pkey = debts.available_primary_key();
        row.debt_id = debt_id;
        row.debtor = debtor;
        row.creditor = creditor;
        row.quantity = quantity;
        row.fee = fee;
        row.profile.clear();
        row.profile = profile;
        row.timestamp = current_block_time();
      });
    }
    else{
      debts.modify(*iterator, get_self(), [&](auto& row){
        row.debt_id = debt_id;
        row.debtor = debtor;
        row.creditor = creditor;
        row.quantity = quantity;
        row.fee = fee;
        row.profile.clear();
        row.profile = profile;
        row.timestamp = current_block_time();
      });
    }
  }


  void transorderdebt::debterase(uint128_t debt_id){
    require_auth(get_self());

    debt_index debts(get_self(), get_self().value);

    auto debt_id_index = debts.get_index<name("bydebtid")>();

    auto iterator = debt_id_index.find(debt_id);

    check(iterator != debt_id_index.end(), "Debt does not exist");

    debt_id_index.erase(iterator);
  }
};