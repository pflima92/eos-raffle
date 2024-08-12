# Raffle Smart Contract

## Summary

This smart contract manages the creation and execution of raffles using EOS tokens on the EOS blockchain. It allows the contract owner to create raffles, update their parameters, assign seats to participants, determine winners through random selection, and handle payouts.

## Actions

### Create

Used to create a new raffle. Only the contract owner (`raffle@active` permission) can execute this action.

**Required Permissions**

Requires `raffle@active` permission.

**Technical Behavior**

Action Parameters:

| Property            | C++ Type     | Required | Remarks                                                    |
|---------------------|--------------|----------|------------------------------------------------------------|
| seats               | uint32_t     | yes      | Defines the number of seats available for the raffle.       |
| seat_price          | eosio::asset | yes      | Defines the price per seat for entering the raffle.         |
| comission_basis_point | uint16_t   | yes      | Defines the commission percentage in basis points (minimum 250). |
| comission_payer     | eosio::name  | yes      | Specifies the account that will receive the commission.     |

### updateseats

Used to increase the number of seats in an ongoing raffle. The number of seats can only be increased, not decreased.

**Required Permissions**

Requires `raffle@active` permission.

**Technical Behavior**

Action Parameters:

| Property  | C++ Type  | Required | Remarks                                      |
|-----------|-----------|----------|----------------------------------------------|
| raffle_id | uint64_t  | yes      | The ID of the raffle to be updated.          |
| new_seats | uint32_t  | yes      | The new total number of seats for the raffle.|

### on_token_transfer

Triggered automatically when tokens are transferred to the contract with a specific raffle ID in the memo. This action assigns a seat to the sender's account if the conditions are met.

**Required Permissions**

Requires `<account>@active` permission.

**Technical Behavior**

Action Parameters:

| Property  | C++ Type     | Required | Remarks                                                                      |
|-----------|--------------|----------|------------------------------------------------------------------------------|
| from      | eosio::name  | yes      | The account transferring the tokens.                                         |
| to        | eosio::name  | yes      | The account receiving the tokens (should be the contract itself).            |
| quantity  | eosio::asset | yes      | The amount of tokens being transferred (should match the seat price).        |
| memo      | std::string  | yes      | The raffle ID for which the seat is being purchased.                         |

### Shuffle

Used to select a winner for a raffle once all conditions are met. This action also handles the distribution of the prize to the winner and the commission to the specified account.

**Required Permissions**

Requires `raffle@active` permission.

**Technical Behavior**

Action Parameters:

| Property  | C++ Type  | Required | Remarks                                      |
|-----------|-----------|----------|----------------------------------------------|
| raffle_id | uint64_t  | yes      | The ID of the raffle to be shuffled.         |

### Invalidate

Used to invalidate a raffle that has no winner yet. This action will erase the raffle and refund any participants who purchased seats.

**Required Permissions**

Requires `raffle@active` permission.

**Technical Behavior**

Action Parameters:

| Property  | C++ Type  | Required | Remarks                                      |
|-----------|-----------|----------|----------------------------------------------|
| raffle_id | uint64_t  | yes      | The ID of the raffle to be invalidated.      |