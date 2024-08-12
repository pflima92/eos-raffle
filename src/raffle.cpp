#include <random.h>
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/print.hpp>

using namespace eosio;
using namespace eosblox;

class [[eosio::contract("raffle")]] raffle : public eosio::contract
{

private:
  const symbol raffle_accepted_symbol;
  const uint16_t contract_shares_basis_point = 250;

  struct [[eosio::table]] raffletb
  {
    uint64_t id;
    uint32_t seats;
    uint32_t allocated_seats;
    asset seat_price;
    bool require_max_seats_allocated;
    uint16_t comission_basis_point; // 1 means 0.0001
    name comission_payer;
    asset funds;
    std::optional<name> winner;

    uint64_t primary_key() const { return id; }
  };

  struct [[eosio::table]] seatstb
  {
    uint64_t id;
    name account;

    uint64_t primary_key() const { return id; }
  };

  using raffle_table = eosio::multi_index<"raffle.d"_n, raffletb>;

  using seats_table = eosio::multi_index<"seats.d"_n, seatstb>;

public:
  raffle(name receiver, name code, datastream<const char *> ds) : contract(receiver, code, ds),
                                                                  raffle_accepted_symbol("UOS", 8)
  {
  }

  [[eosio::action]] void create(uint32_t seats, asset seat_price, uint16_t comission_basis_point, name comission_payer)
  {
    require_auth(get_self());

    eosio::check(seats > 1, "minimum of 2 seats are required");
    eosio::check(seat_price.symbol == raffle_accepted_symbol, "seat price must be a UOS symbol");
    eosio::check(comission_basis_point > 250, "comissions basis point should be at least 250");

    raffle_table _raffle(get_self(), get_self().value);

    // prevent the new id to be 0 - which is default value for uint64_t
    auto new_id = _raffle.available_primary_key();
    new_id = new_id == 0 ? 1 : new_id;
    _raffle.emplace(get_self(), [&](auto &new_raffle)
                    {
                      new_raffle.id = new_id;
                      new_raffle.seats = seats;
                      new_raffle.seat_price = seat_price;
                      new_raffle.comission_basis_point = comission_basis_point;
                      new_raffle.comission_payer = comission_payer;
                      new_raffle.funds = asset(0, raffle_accepted_symbol); });
  }

  [[eosio::action]] void updateseats(uint64_t raffle_id, uint32_t new_seats)
  {
    require_auth(get_self());

    raffle_table r_table(get_self(), get_self().value);
    auto r_itr = r_table.find(raffle_id);

    eosio::check(r_itr != r_table.end(), "raffle id does not exist");
    eosio::check(r_itr->seats <= new_seats, "the new number of seats cannot be lower or equal the actual seats");

    r_table.modify(r_itr, same_payer, [&](auto &r)
                   { r.seats = new_seats; });
  }

  [[eosio::on_notify("eosio.token::transfer")]] void on_token_transfer(name from, name to, eosio::asset quantity, std::string memo)
  {
    if (to != get_self() || memo.empty())
    {
      return;
    }

    auto raffle_id = atoi(memo.c_str());
    raffle_table r_table(get_self(), get_self().value);
    auto r_itr = r_table.find(raffle_id);

    eosio::check(r_itr != r_table.end(), "raffle id does not exist");
    eosio::check(!r_itr->winner.has_value(), "this raffle already have a winner");
    eosio::check(r_itr->seats >= r_itr->allocated_seats + 1, "max seats allocated");

    // when using on_notify we need to check if the quantity transferred is valid to buy a seat
    eosio::check(quantity >= r_itr->seat_price, "quantity received is lower than seat price for this raffle");

    // assign seat to account
    seats_table s_table(get_self(), raffle_id);

    s_table.emplace(get_self(), [&](auto &new_seat)
                    { new_seat.id = s_table.available_primary_key();
                                        new_seat.account = from; });

    r_table.modify(r_itr, same_payer, [&](auto &r)
                   { r.allocated_seats++;
      r.funds += quantity; });
  }

  [[eosio::action]] void shuffle(uint64_t raffle_id)
  {
    require_auth(get_self());

    raffle_table r_table(get_self(), get_self().value);
    auto r_itr = r_table.find(raffle_id);

    eosio::check(r_itr != r_table.end(), "raffle id does not exist");
    eosio::check(!r_itr->winner.has_value(), "this raffle already have a winner");

    if (r_itr->require_max_seats_allocated)
    {
      eosio::check(r_itr->allocated_seats == r_itr->seats, "seats are not full allocated yet");
    }

    // comission to pay to the account define in the smart countract
    auto comission_to_pay = asset((r_itr->funds.amount * r_itr->comission_basis_point) / 10000, raffle_accepted_symbol);
    auto smart_contract_fee_to_pay = asset((r_itr->funds.amount * contract_shares_basis_point) / 10000, raffle_accepted_symbol);
    auto winner_pot = r_itr->funds - comission_to_pay - smart_contract_fee_to_pay;

    // share comission
    action{
        permission_level{get_self(), "active"_n},
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple(get_self(), r_itr->comission_payer, comission_to_pay, std::string("[@uosloterry.raffle] comission shares"))}
        .send();

    // identify and pay the winner
    std::vector<name> seats;
    seats.reserve(r_itr->allocated_seats);

    seats_table s_table(get_self(), raffle_id);

    // suffle the raffle
    Random gen;
    auto shuffled = gen.nextInRange(0, r_itr->allocated_seats);
    auto s_itr = s_table.find(shuffled);

    eosio::check(s_itr != s_table.end(), "shuddled id does not exist");

    // Get account for the winner
    name winner = s_itr->account;
    

    // // the quantity to be sent to the winner is the different between the pot of funds and the commistion to share
    action{
        permission_level{get_self(), "active"_n},
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple(get_self(), winner, winner_pot, std::string("[@uosloterry.raffle] winner shares"))}
        .send();

    r_table.modify(r_itr, same_payer, [&](auto &r)
                   { r.winner = winner; });
  }

  // TODO only for develop
  [[eosio::action]] void invalidate(uint64_t raffle_id)
  {
    require_auth(get_self());

    raffle_table _raffle(get_self(), get_self().value);
    auto r_itr = _raffle.find(raffle_id);

    eosio::check(r_itr != _raffle.end(), "raffle id does not exist");
    eosio::check(!r_itr->winner.has_value(), "cannot invalidate a raffle with a winner");

    _raffle.erase(r_itr);
  }
};
