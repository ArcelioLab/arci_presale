#include "arci_presale.hpp"

#include "config.h"


// constructor
arci_presale::arci_presale(name self, name code, datastream<const char *> ds) : contract(self, code, ds),
                                                                            state_singleton(this->_self, this->_self.value), // code and scope both set to the contract's account
                                                                            reserved_singleton(this->_self, this->_self.value), // code and scope both set to the contract's account
                                                                            deposits(this->_self, this->_self.value),
                                                                            state(state_singleton.exists() ? state_singleton.get() : default_parameters()),
                                                                            reserved(reserved_singleton.exists() ? reserved_singleton.get() : default_parameters1())
                                                                            {}

// destructor
arci_presale::~arci_presale()
{
    this->state_singleton.set(this->state, this->_self); // persist the state of the crowdsale before destroying instance

    print(" Saving state to the RAM ");
    print(this->state.toString());

    this->reserved_singleton.set(this->reserved, this->_self); // persist the state of the crowdsale before destroying instance

    print(" Saving state to the RAM ");
    print(this->reserved.toString());    
}

// initialize the crowdfund
void arci_presale::init(name recipient, time_point_sec start, time_point_sec finish)
{
    // check(!this->state_singleton.exists(), "Already Initialzed");
    check(start < finish, "Start must be less than finish");
    require_auth(this->_self);
    check(recipient != this->_self,"Recipient should be different than contract deployer");
    // update state
    this->state.recipient = recipient;
    this->state.start = start;
    this->state.finish = finish;
    this->state.pause = false;
    
    asset goal = asset(GOAL, symbol("ARCI", 6));
}

// update contract balances and send arci tokens to the investor
void arci_presale::handle_investment(name investor, uint64_t tokens_to_give)
{
    // hold the reference to the investor stored in the RAM
    auto it = this->deposits.find(investor.value);
    
    // if the depositor account was found, store his updated balances
    asset entire_tokens = asset(tokens_to_give, symbol("ARCI", 6));

        if (it != this->deposits.end())
        {
             // MAX_BUY_PER_ACCOUNT = 100 * 10000; untuk token decimal 4

                check(entire_tokens.amount + it->tokens.amount <= MAX_BUY_PER_ACCOUNT, "Your account exceeds the maximum purchase");
                entire_tokens += it->tokens;
        }

        // if the depositor was not found create a new entry in the database, else update his balance
        if (it == this->deposits.end())
        {   
            
            this->deposits.emplace(this->_self, [investor, entire_tokens](auto &deposit) {
                
                deposit.account = investor;
                deposit.tokens = entire_tokens;
            });
        }
        else
        {
            this->deposits.modify(it, this->_self, [investor, entire_tokens](auto &deposit) {
               
                deposit.account = investor;
                deposit.tokens = entire_tokens;
            });
        }
}


void arci_presale::cleardeposit(uint64_t limit)
{
	require_auth(_self);

	uint64_t count = 0;
	for (auto itr = this->deposits.begin(); itr != this->deposits.end() && count < limit;) {
		itr = this->deposits.erase(itr);
	        count++;
	}
	print("menghapus ", count,  " baris data");
}

// handle transfers to this contract
void arci_presale::buyarci(name from, name to, asset quantity, std::string memo)
{
    print(from);
    print(to);
    print(quantity);
    print(memo);
        
    if((to == this->_self)&&(quantity.symbol==sy_vex)){  

    check(this->state.pause==false, "Presale is paused" );
    
    // check timings of the eos crowdsale
    check(current_time_point().sec_since_epoch() >= this->state.start.utc_seconds, "Presale hasn't started yet");

    // check if the Goal was reached
    check(this->state.total_tokens.amount <= GOAL, "GOAL reached");

    //calculate 3% fees on buying arci
    // asset fees ;
    // fees.amount = quantity.amount*0.03;
    // fees.symbol = sy_vex;
    
    this->state.total_vex.amount += quantity.amount;

    // quantity-=fees;
    // dont send fees to _self
    // else arci supply would increase
    
    int64_t tokens_to_give; 
    if(current_time_point().sec_since_epoch() <= this->state.finish.utc_seconds){
        // check the minimum and maximum contribution
        check(quantity.amount >= MIN_CONTRIB, "Your contribution in phase 2 is too low. Min contribution is 14000 VEX.");
        check(quantity.amount <= MAX_CONTRIB, "Your contribution in phase 2 is too high. Max contribution is 280000 VEX.");

        // check if the Goal was reached
        // check(this->state.total_tokens.amount <= GOAL1, "PHASE 1 Goal reached");
        tokens_to_give = (quantity.amount)/RATE1;
    }
    else {
        check(quantity.amount >= MIN_CONTRIB2, "Your contribution in phase 3 is too low. Min contribution is 72000 VEX.");
        check(quantity.amount <= MAX_CONTRIB2, "Your contribution in phase 3 is too high. Max contribution is 1440000 VEX.");
        // check(this->state.total_tokens.amount <= GOAL2, "PHASE 2 Goal reached");
        tokens_to_give = (quantity.amount)/RATE2;
    }
    // calculate from VEX to tokens
    this->state.total_tokens.amount += tokens_to_give; // calculate from VEX to tokens

    // handle investment
    this->handle_investment(from, tokens_to_give);
    
    // set the amounts to transfer, then call inline issue action to update balances in the token contract
    asset amount = asset(tokens_to_give, symbol("ARCI", 6));
    
    this->inline_transfer(this->_self, from, amount, "Thank you for participating in the ARCI token early supporters program 🤝"); 
    }
}

// used for trading.
void arci_presale::transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo)
{
    require_auth(from);

    // calculate fees and send by rate...
    this->inline_transfer(from, to, quantity, "trading ");
  
}
// toggles unpause / pause contract
void arci_presale::pause()
{
    require_auth(this->state.recipient);
    if (state.pause==false)
        this->state.pause = true; 
    else
        this->state.pause = false;
}

void arci_presale::rate()
{
    print("Rate of 1 ARCI token: ");
    print(RATE1);
}

void arci_presale::checkgoal()
{
    if(this->state.total_tokens.amount <= GOAL)
    {
        print("Goal not Reached: ");
        print(GOAL);
    }
    else
        print("Goal Reached");
}

// custom dispatcher that handles ARCI transfers from tesarcitoken contract
extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action)
{
    if (code == eosio::name("vex.token").value && action == eosio::name("transfer").value) // handle actions from eosio.token contract
    {
        eosio::execute_action(eosio::name(receiver), eosio::name(code), &arci_presale::buyarci);
    }
    else if (code == receiver) // for other direct actions
    {
        switch (action)
        {
            EOSIO_DISPATCH_HELPER(arci_presale, (init)(cleardeposit)(transfer)(pause)(rate)(checkgoal));
        }
    }
}
