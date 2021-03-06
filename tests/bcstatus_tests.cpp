//
// Created by mwo on 15/06/18.
//

#include "../src/MicroCore.h"
#include "../src/CurrentBlockchainStatus.h"
#include "../src/ThreadRAII.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "helpers.h"

namespace
{


using json = nlohmann::json;
using namespace std;
using namespace cryptonote;
using namespace epee::string_tools;
using namespace std::chrono_literals;

using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Le;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using ::testing::Throw;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::_;
using ::testing::internal::FilePath;


class MockMicroCore : public xmreg::MicroCore
{
public:    
    MOCK_METHOD2(init, bool(const string& _blockchain_path,
                            network_type nt));

    MOCK_CONST_METHOD0(get_current_blockchain_height, uint64_t());

    MOCK_CONST_METHOD2(get_block_from_height,
                       bool(uint64_t height, block& blk));

    MOCK_CONST_METHOD2(get_blocks_range,
                       std::vector<block>(const uint64_t& h1,
                                          const uint64_t& h2));

    MOCK_CONST_METHOD3(get_transactions,
                       bool(const std::vector<crypto::hash>& txs_ids,
                            std::vector<transaction>& txs,
                            std::vector<crypto::hash>& missed_txs));

    MOCK_CONST_METHOD1(have_tx, bool(crypto::hash const& tx_hash));

    MOCK_CONST_METHOD2(tx_exists,
                       bool(crypto::hash const& tx_hash,
                            uint64_t& tx_id));

    MOCK_CONST_METHOD2(get_output_tx_and_index,
                       tx_out_index(uint64_t const& amount,
                                    uint64_t const& index));

    MOCK_CONST_METHOD2(get_tx,
                       bool(crypto::hash const& tx_hash,
                            transaction& tx));

    MOCK_METHOD3(get_output_key,
                    void(const uint64_t& amount,
                         const vector<uint64_t>& absolute_offsets,
                         vector<cryptonote::output_data_t>& outputs));

    MOCK_METHOD2(get_output_key,
                    output_data_t(uint64_t amount,
                                  uint64_t global_amount_index));

    MOCK_CONST_METHOD1(get_tx_amount_output_indices,
                    std::vector<uint64_t>(uint64_t const& tx_id));

    MOCK_CONST_METHOD2(get_random_outs_for_amounts,
                        bool(COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request const& req,
                             COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res));

    MOCK_CONST_METHOD2(get_outs,
                        bool(const COMMAND_RPC_GET_OUTPUTS_BIN::request& req,
                             COMMAND_RPC_GET_OUTPUTS_BIN::response& res));

    MOCK_CONST_METHOD1(get_dynamic_per_kb_fee_estimate,
                       uint64_t(uint64_t const& grace_blocks));

    MOCK_CONST_METHOD2(get_mempool_txs,
                       bool(vector<tx_info>& tx_infos,
                            vector<spent_key_image_info>& key_image_infos));

      // dont need to mock this function currently
//    MOCK_METHOD3(decrypt_payment_id,
//                       bool(crypto::hash8& payment_id,
//                            crypto::public_key const& public_key,
//                            crypto::secret_key const& secret_key));

};

class MockRPCCalls : public xmreg::RPCCalls
{
public:
    MockRPCCalls(string _deamon_url)
        : xmreg::RPCCalls(_deamon_url)
    {}

    MOCK_METHOD3(commit_tx, bool(const string& tx_blob,
                                 string& error_msg,
                                 bool do_not_relay));
};

class MockTxSearch : public xmreg::TxSearch
{
public:
    MOCK_METHOD0(operator_fcall, void());
    void operator()() override {operator_fcall();}

    MOCK_METHOD0(ping, void());

    MOCK_CONST_METHOD0(still_searching, bool());

    MOCK_CONST_METHOD0(get_searched_blk_no, uint64_t());

    MOCK_METHOD0(get_known_outputs_keys,
                 xmreg::TxSearch::known_outputs_t());

    MOCK_CONST_METHOD0(get_xmr_address_viewkey,
                 xmreg::TxSearch::addr_view_t());
};


class BCSTATUS_TEST : public ::testing::TestWithParam<network_type>
{
public:

    static void
    SetUpTestCase()
    {
        string config_path {"../config/config.json"};
        config_json = xmreg::BlockchainSetup::read_config(config_path);
    }

protected:

    virtual void
    SetUp()
    {
        net_type = GetParam();

        bc_setup = xmreg::BlockchainSetup {
                net_type, do_not_relay, config_json};

        mcore = std::make_unique<MockMicroCore>();
        mcore_ptr = mcore.get();

        rpc = std::make_unique<MockRPCCalls>("dummy deamon url");
        rpc_ptr = rpc.get();

        bcs = std::make_unique<xmreg::CurrentBlockchainStatus>(
                    bc_setup, std::move(mcore), std::move(rpc));
    }

     network_type net_type {network_type::STAGENET};
     bool do_not_relay {false};
     xmreg::BlockchainSetup bc_setup;
     std::unique_ptr<MockMicroCore> mcore;
     std::unique_ptr<MockRPCCalls> rpc;
     std::unique_ptr<xmreg::CurrentBlockchainStatus> bcs;

     MockMicroCore* mcore_ptr;
     MockRPCCalls* rpc_ptr;

     static json config_json;
};


json BCSTATUS_TEST::config_json;

TEST_P(BCSTATUS_TEST, DefaultConstruction)
{
    xmreg::CurrentBlockchainStatus bcs {bc_setup, nullptr, nullptr};
    EXPECT_TRUE(true);
}



TEST_P(BCSTATUS_TEST, InitMoneroBlockchain)
{
    EXPECT_CALL(*mcore_ptr, init(_, _))
            .WillOnce(Return(true));

    EXPECT_TRUE(bcs->init_monero_blockchain());
}

TEST_P(BCSTATUS_TEST, GetBlock)
{
   EXPECT_CALL(*mcore_ptr, get_block_from_height(_, _))
           .WillOnce(Return(true));

    uint64_t height = 1000;
    block blk;

    EXPECT_TRUE(bcs->get_block(height, blk));
}


ACTION(ThrowBlockDNE)
{
    throw BLOCK_DNE("Mock Throw: Block does not exist!");
}

TEST_P(BCSTATUS_TEST, GetBlockRange)
{

   vector<block> blocks_to_return {block(), block(), block()};

   EXPECT_CALL(*mcore_ptr, get_blocks_range(_, _))
           .WillOnce(Return(blocks_to_return));

    uint64_t h1 = 1000;
    uint64_t h2 = h1+2;

    vector<block> blocks = bcs->get_blocks_range(h1, h2);

    EXPECT_EQ(blocks, blocks_to_return);

    EXPECT_CALL(*mcore_ptr, get_blocks_range(_, _))
            .WillOnce(ThrowBlockDNE());

    blocks = bcs->get_blocks_range(h1, h2);

    EXPECT_TRUE(blocks.empty());
}

TEST_P(BCSTATUS_TEST, GetBlockTxs)
{
    EXPECT_CALL(*mcore_ptr, get_transactions(_, _, _))
            .WillOnce(Return(true));

    const block dummy_blk;
    vector<transaction> blk_txs;
    vector<crypto::hash> missed_txs;

    EXPECT_TRUE(bcs->get_block_txs(dummy_blk, blk_txs, missed_txs));

    EXPECT_CALL(*mcore_ptr, get_transactions(_, _, _))
            .WillOnce(Return(false));

    EXPECT_FALSE(bcs->get_block_txs(dummy_blk, blk_txs, missed_txs));
}

TEST_P(BCSTATUS_TEST, GetTxs)
{
    EXPECT_CALL(*mcore_ptr, get_transactions(_, _, _))
            .WillOnce(Return(true));

    vector<crypto::hash> txs_to_get;
    vector<transaction> blk_txs;
    vector<crypto::hash> missed_txs;

    EXPECT_TRUE(bcs->get_txs(txs_to_get, blk_txs, missed_txs));

    EXPECT_CALL(*mcore_ptr, get_transactions(_, _, _))
            .WillOnce(Return(false));

    EXPECT_FALSE(bcs->get_txs(txs_to_get, blk_txs, missed_txs));
}

TEST_P(BCSTATUS_TEST, TxExist)
{
    EXPECT_CALL(*mcore_ptr, have_tx(_))
            .WillOnce(Return(true));

    EXPECT_TRUE(bcs->tx_exist(crypto::hash()));

    uint64_t mock_tx_index_to_return {4444};

    // return true and set tx_index (ret by ref) to mock_tx_index_to_return
    EXPECT_CALL(*mcore_ptr, tx_exists(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(mock_tx_index_to_return),
                            Return(true)));

    uint64_t tx_index {0};

    EXPECT_TRUE(bcs->tx_exist(crypto::hash(), tx_index));
    EXPECT_EQ(tx_index, mock_tx_index_to_return);

    // just some dummy hash
    string tx_hash_str
        {"fc4b8d5956b30dc4a353b171b4d974697dfc32730778f138a8e7f16c11907691"};

    tx_index = 0;

    EXPECT_CALL(*mcore_ptr, tx_exists(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(mock_tx_index_to_return),
                            Return(true)));

    EXPECT_TRUE(bcs->tx_exist(tx_hash_str, tx_index));
    EXPECT_EQ(tx_index, mock_tx_index_to_return);

    tx_hash_str = "wrong_hash";

    EXPECT_FALSE(bcs->tx_exist(tx_hash_str, tx_index));
}


TEST_P(BCSTATUS_TEST, GetTxWithOutput)
{
    // some dummy tx hash
    RAND_TX_HASH();

    const tx_out_index tx_idx_to_return = make_pair(tx_hash, 6);

    EXPECT_CALL(*mcore_ptr, get_output_tx_and_index(_, _))
            .WillOnce(Return(tx_idx_to_return));

    EXPECT_CALL(*mcore_ptr, get_tx(_, _))
            .WillOnce(Return(true));

    const uint64_t mock_output_idx {4};
    const uint64_t mock_amount {11110};

    transaction tx_returned;
    uint64_t out_idx_returned;

    EXPECT_TRUE(bcs->get_tx_with_output(mock_output_idx, mock_amount,
                                        tx_returned, out_idx_returned));
}

ACTION(ThrowOutputDNE)
{
    throw OUTPUT_DNE("Mock Throw: Output does not exist!");
}

TEST_P(BCSTATUS_TEST, GetTxWithOutputFailure)
{
    // some dummy tx hash
    RAND_TX_HASH();

    const tx_out_index tx_idx_to_return = make_pair(tx_hash, 6);

    EXPECT_CALL(*mcore_ptr, get_output_tx_and_index(_, _))
            .WillOnce(Return(tx_idx_to_return));

    EXPECT_CALL(*mcore_ptr, get_tx(_, _))
            .WillOnce(Return(false));

    const uint64_t mock_output_idx {4};
    const uint64_t mock_amount {11110};

    transaction tx_returned;
    uint64_t out_idx_returned;

    EXPECT_FALSE(bcs->get_tx_with_output(mock_output_idx, mock_amount,
                                        tx_returned, out_idx_returned));

    // or

    EXPECT_CALL(*mcore_ptr, get_output_tx_and_index(_, _))
            .WillOnce(ThrowOutputDNE());

    EXPECT_FALSE(bcs->get_tx_with_output(mock_output_idx, mock_amount,
                                        tx_returned, out_idx_returned));
}

TEST_P(BCSTATUS_TEST, GetCurrentHeight)
{
    uint64_t mock_current_height {1619148};

    EXPECT_CALL(*mcore_ptr, get_current_blockchain_height())
            .WillOnce(Return(mock_current_height));

    bcs->update_current_blockchain_height();

    EXPECT_EQ(bcs->get_current_blockchain_height(),
              mock_current_height - 1);
}

TEST_P(BCSTATUS_TEST, IsTxSpendtimeUnlockedScenario1)
{
    // there are two main scenerious here.
    // Scenerio 1: tx_unlock_time is block height
    // Scenerio 2: tx_unlock_time is timestamp.

    const uint64_t mock_current_height {100};

    EXPECT_CALL(*mcore_ptr, get_current_blockchain_height())
            .WillOnce(Return(mock_current_height));

    bcs->update_current_blockchain_height();

    // SCENARIO 1: tx_unlock_time is block height

    // expected unlock time is in future, thus a tx is still locked

    uint64_t tx_unlock_time {mock_current_height
                + CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE};

    uint64_t not_used_block_height {0}; // not used in the first
                                        // part of the test case

    EXPECT_FALSE(bcs->is_tx_unlocked(
                     tx_unlock_time, not_used_block_height));

    // expected unlock time is in the future
    // (1 blocks from now), thus a tx is locked

    tx_unlock_time = mock_current_height + 1;

    EXPECT_FALSE(bcs->is_tx_unlocked(
                     tx_unlock_time, not_used_block_height));

    // expected unlock time is in the past
    // (10 blocks behind), thus a tx is unlocked

    tx_unlock_time = mock_current_height
            - CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE;

    EXPECT_TRUE(bcs->is_tx_unlocked(tx_unlock_time,
                                              not_used_block_height));

    // expected unlock time is same as as current height
    // thus a tx is unlocked

    tx_unlock_time = mock_current_height;

    EXPECT_TRUE(bcs->is_tx_unlocked(tx_unlock_time,
                                              not_used_block_height));
}


class MockTxUnlockChecker : public xmreg::TxUnlockChecker
{
public:

    // mock system call to get current timestamp
    MOCK_CONST_METHOD0(get_current_time, uint64_t());
    //MOCK_CONST_METHOD1(get_leeway, uint64_t(uint64_t tx_block_height));
};

TEST_P(BCSTATUS_TEST, IsTxSpendtimeUnlockedScenario2)
{
    // there are two main scenerious here.
    // Scenerio 1: tx_unlock_time is block height
    // Scenerio 2: tx_unlock_time is timestamp.

    const uint64_t mock_current_height {100};

    EXPECT_CALL(*mcore_ptr, get_current_blockchain_height())
            .WillOnce(Return(mock_current_height));

    bcs->update_current_blockchain_height();

    // SCENARIO 2: tx_unlock_time is timestamp.

    MockTxUnlockChecker mock_tx_unlock_checker;

    const uint64_t current_timestamp {1000000000};

    EXPECT_CALL(mock_tx_unlock_checker, get_current_time())
            .WillRepeatedly(Return(1000000000));

    uint64_t block_height = mock_current_height;

    // tx unlock time is now
    uint64_t tx_unlock_time {current_timestamp}; // mock timestamp

    EXPECT_TRUE(bcs->is_tx_unlocked(tx_unlock_time, block_height,
                                    mock_tx_unlock_checker));

    // unlock time is 1 second more than needed
    tx_unlock_time = current_timestamp
            + mock_tx_unlock_checker.get_leeway(
                block_height, bcs->get_bc_setup().net_type) + 1;

    EXPECT_FALSE(bcs->is_tx_unlocked(tx_unlock_time, block_height,
                                     mock_tx_unlock_checker));

}


TEST_P(BCSTATUS_TEST, GetOutputKeys)
{
    // we are going to expect two outputs
    vector<output_data_t> outputs_to_return;

    outputs_to_return.push_back(
                output_data_t {
                crypto::rand<crypto::public_key>(),
                1000, 2222,
                crypto::rand<rct::key>()});

    outputs_to_return.push_back(
                output_data_t {
                crypto::rand<crypto::public_key>(),
                3333, 5555,
                crypto::rand<rct::key>()});

    EXPECT_CALL(*mcore_ptr, get_output_key(_, _, _))
            .WillOnce(SetArgReferee<2>(outputs_to_return));

    const uint64_t mock_amount {1111};
    const vector<uint64_t> mock_absolute_offsets;
    vector<cryptonote::output_data_t> outputs;

    EXPECT_TRUE(bcs->get_output_keys(mock_amount,
                                     mock_absolute_offsets,
                                     outputs));

    EXPECT_EQ(outputs.back().pubkey, outputs_to_return.back().pubkey);

    EXPECT_CALL(*mcore_ptr, get_output_key(_, _, _))
            .WillOnce(ThrowOutputDNE());

    EXPECT_FALSE(bcs->get_output_keys(mock_amount,
                                      mock_absolute_offsets,
                                      outputs));
}

TEST_P(BCSTATUS_TEST, GetAccountIntegratedAddressAsStr)
{
    // bcs->get_account_integrated_address_as_str only forwards
    // call to cryptonote function. so we just check if
    // forwarding is correct, not wether the cryptonote
    // function works correctly.

    crypto::hash8 payment_id8 = crypto::rand<crypto::hash8>();
    string payment_id8_str = pod_to_hex(payment_id8);

    string expected_int_addr
            = cryptonote::get_account_integrated_address_as_str(
                bcs->get_bc_setup().net_type,
                bcs->get_bc_setup().import_payment_address.address,
                payment_id8);

    string resulting_int_addr
            = bcs->get_account_integrated_address_as_str(payment_id8);

    EXPECT_EQ(expected_int_addr, resulting_int_addr);

    resulting_int_addr
                = bcs->get_account_integrated_address_as_str(
                payment_id8_str);

    EXPECT_EQ(expected_int_addr, resulting_int_addr);


    resulting_int_addr
                = bcs->get_account_integrated_address_as_str(
                "wrong_payment_id8");

    EXPECT_TRUE(resulting_int_addr.empty());
}


ACTION(ThrowTxDNE)
{
    throw TX_DNE("Mock Throw: Tx does not exist!");
}

TEST_P(BCSTATUS_TEST, GetAmountSpecificIndices)
{
    vector<uint64_t> out_indices_to_return {1,2,3};

    EXPECT_CALL(*mcore_ptr, tx_exists(_, _))
            .WillOnce(Return(true));

    EXPECT_CALL(*mcore_ptr, get_tx_amount_output_indices(_))
            .WillOnce(Return(out_indices_to_return));

    vector<uint64_t> out_indices;

    RAND_TX_HASH();

    EXPECT_TRUE(bcs->get_amount_specific_indices(tx_hash, out_indices));

    EXPECT_EQ(out_indices, out_indices_to_return);

    EXPECT_CALL(*mcore_ptr, tx_exists(_, _))
            .WillOnce(Return(false));

    EXPECT_FALSE(bcs->get_amount_specific_indices(tx_hash, out_indices));

    EXPECT_CALL(*mcore_ptr, tx_exists(_, _))
            .WillOnce(ThrowTxDNE());

    EXPECT_FALSE(bcs->get_amount_specific_indices(tx_hash, out_indices));
}

TEST_P(BCSTATUS_TEST, GetRandomOutputs)
{
    using out_for_amount = COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS
                                    ::outs_for_amount;

    std::vector<out_for_amount> outputs_to_return;

    outputs_to_return.push_back(out_for_amount {22, {}});
    outputs_to_return.push_back(out_for_amount {66, {}});

    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response res;

    res.outs = outputs_to_return;

    EXPECT_CALL(*mcore_ptr, get_random_outs_for_amounts(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(res), Return(true)));

    const vector<uint64_t> mock_amounts {444, 556, 77}; // any
    const uint64_t mock_outs_count {3}; // any

    std::vector<out_for_amount> found_outputs;

    EXPECT_TRUE(bcs->get_random_outputs(
                    mock_amounts, mock_outs_count,
                    found_outputs));

    EXPECT_EQ(found_outputs.size(), outputs_to_return.size());
    EXPECT_EQ(found_outputs.back().amount,
              outputs_to_return.back().amount);

    EXPECT_CALL(*mcore_ptr, get_random_outs_for_amounts(_, _))
            .WillOnce(Return(false));

    EXPECT_FALSE(bcs->get_random_outputs(
                    mock_amounts, mock_outs_count,
                    found_outputs));
}

TEST_P(BCSTATUS_TEST, GetOutput)
{
    using outkey = COMMAND_RPC_GET_OUTPUTS_BIN::outkey;

    outkey output_key_to_return {
        crypto::rand<crypto::public_key>(),
        crypto::rand<rct::key>(),
        true, 444,
        crypto::rand<crypto::hash>()};

    COMMAND_RPC_GET_OUTPUTS_BIN::response res;

    res.outs.push_back(output_key_to_return);

    EXPECT_CALL(*mcore_ptr, get_outs(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(res), Return(true)));

    const uint64_t mock_amount {0};
    const uint64_t mock_global_output_index {0};
    outkey output_info;

    EXPECT_TRUE(bcs->get_output(mock_amount,
                                mock_global_output_index,
                                output_info));

    EXPECT_EQ(output_info.key, output_key_to_return.key);

    EXPECT_CALL(*mcore_ptr, get_outs(_, _))
            .WillOnce(Return(false));

    EXPECT_FALSE(bcs->get_output(mock_amount,
                                mock_global_output_index,
                                output_info));
}

TEST_P(BCSTATUS_TEST, GetDynamicPerKbFeeEstimate)
{
    EXPECT_CALL(*mcore_ptr, get_dynamic_per_kb_fee_estimate(_))
            .WillOnce(Return(3333));

    EXPECT_EQ(bcs->get_dynamic_per_kb_fee_estimate(), 3333);
}

TEST_P(BCSTATUS_TEST, CommitTx)
{
    EXPECT_CALL(*rpc_ptr, commit_tx(_, _, _))
            .WillOnce(Return(true));

    string tx_blob {"mock blob"};
    string error_msg;

    EXPECT_TRUE(bcs->commit_tx(tx_blob, error_msg, true));

    EXPECT_CALL(*rpc_ptr, commit_tx(_, _, _))
            .WillOnce(Return(false));

    EXPECT_FALSE(bcs->commit_tx(tx_blob, error_msg, true));
}


TEST_P(BCSTATUS_TEST, ReadMempool)
{
    // stagenet tx: 4b40cfb2fdce2cd57a834a380901d55d70aba29dad13ac6c4dc82a895f439ecf
    const string tx_4b40_hex {"0200010200089832f68b01b2a601a21ec01da83ffe0139a71678019703665175d1e067d20c738a8634aeaad6a7bc493a4b5a641b4962fa020002fb1d79f31791c9553c6dc1c066cde2361385021a5c33e7101d69a557cc3c34a1000292b1073ac7c2fdd851955e7fb76cbc7de4e812db745b5e795445594ea1c7d2a721010a9e82db48149442ac50cff52a39fda95f90b9683098b02e413a52b4d1f05f1c0180f3a3f93aaac0d8887ce542c01e49afbaf1f5ac8e1e4c8882011dd83fa33d1c9efa5f210d39e17c153a6a5825da329dcb31cd1acae98961d55038fb22e78fb93b95e6820a6904ff5b7bbfcf0cd1f6b9e9dd56f41062c7f5f43d9e1b3d69724eeca4bc3404a41ff0669f7f8ae725da8bbc2f8d0411605d2c3e959e3099414ffce1654418020d6ee74bb25042045e955b966e060a9f65b9cfccfbdcb4ee3101019a51f3a41372414c90383c9b84fb4ac690047ea67d70c917192abeb1fdf6c618aeafc732ae9a3e6c6f7fb274240ca04d24035a1d55b3fd1060dc3f4e64f9816868f36d2a0aeb76b64169bc9525d7d70b077c8b998b1204c6108e2917af893b0de24f4adb043eee433b096db8ef730de6e339a84f966b80b239821f350e2a8b10817fc19508da4f82bcb90fcf84cd47929ab1ba90bde5d252767736a4b23d26063cc6b2120dc636b2d4e3e7b3f6d7448882e01c10cf7bdd2117710dc49a59b49cf99212800093da22a6ab5bf693a80d4a662f35259f088a91a26ac261980c537238daa7a40477871f7481973edbcc86fa4885a4061d5c889d0986b4c0cb394165d4039081069f61cff822c7a227474d1609dbf05909f738803834b9e1b5ee68dcb825fcd906d880684e5fa684c8cb02495530fcaeeb35a405345f352ef3c8cf276c096d4303ee14cc0a48533afd1bcdf32dcfb25db4c45a19b10926dff72ace2c2a1d0759090acff99ad06e3f81c4d759a9f74d47fff9cb61d3784aa0eb57a250eec8d14800b85026f5b112740e2e8c257541bdfa4ea1f166b9d930e9705fa1d3f3c2be1e0fb242394d9230c7c457fc8422a6f919a06df5c52d954fa1bfdb57a0a2778d35020ef07490fc3c4f6e99ceaef97fcb3da049898944f3291b96a4f9ce491654340414565db2e0c6bd13eab933194b32336c09c478689cc4a7160b63ecb59937b2028e1649744c6ea69db01aed72fb8b39519cb28d764f158789cc539e01fd5f8f098597c8977e6d2b22c37b289c95dca13bece9e28f741ae2384ead8f0d6df7c30f311210523cb0b7e8c1173aee311362282f3c7d06ae2153969d073bec14ff3605d193bfe829e110df39b894fc4a8eb462d357098688748d579c85f9dc9e9cd20229c9865bc54ae8338433247c28875249e37d4d9b9ccbad611ce5948f1c7ea00fccdc41a9fbe942539a44111f160c9b1dc2653ecae0e4c30bada58d9164fa4c021b60fc0e2068510ef8ec56f7c8e036f2eb31ae0871c9a6aff7b8ad6a86054a06daf06393871f41859f51165f09d22fedd8a2326099a803491dbff5027f72540ed6981f505d1162d86c1ec716758bfbf4e1bfdbbe628b9340942a328209c86b0ec71981c90b407b146cbf065cfd8b0a1c2528aaf007c505a83779dacf8cb7500577a1f8f4a2c9a833d08b254c0c5edbbe87c9eaf266627d23bf518373dceba4099c13c6c468b99c16d56ea5eec12620867fdeb01be66bb0248a7ab7dd6434aa042392c4482e8694292bfc638f1a344d35c9cbfe121a25d2302db4db9688285905fcef9a892593840fe2ddebf0e059e341b2e7f107ee6a19fe386e3f9ea0da2801c5e9fe1b9c4b21155f6f2194ef36f2383097f9cf7ee0994873cfd92d098a33006547ef67bbeb2c1386fcbeec6f0b974532f2ac6e474b26683a37ed3919ec3108b578d3a552c32a362c0bea8bfe35c481f014721af9503a0663d95f5aa91b8802c32f60dbaa65978ad114a2fd45901a07fb4dded61d19e30c8c11e4efbdc1180a02d08d05a646bd8a4d160203da8d490ae0a0b4e631280401d635eb78e0416408c208436dcd653c11a75f365e2d012e0e69f0d4fe2fb2997726dad29468109001ed7c65464ba734e4b35f2ac85a4437fcafe0cdb6128e634970ba420e2e3296080636520e9fbf453e07cdd215117a2eeffed5a0ea0003761f5eb4c35aebf87b063f59d0571e149dc9d0c143a59415a7fe950d34b5024087145969abdc7f7140079bc25119825dda1bf080150bd40142d9635fd87e5dc691899d2686ccf6c9d106c7cdbf783fbbc83fc0feebe8abee3b458e7417a385215f1f89a2a89bef3b870c7a30a6cf4920ddd6eb50439c95c1f38fec867a41e628415ed142b7014991fd0818e8cf725b7075498e773790ccc882db0eb22453f0c9931debfb8d3adb09370f30e62b5fe94ffa7e55ff053488ad48f6e244ed6f043fae8f2225a024800c690ce15dd8017457d23c834b276d079d4a141f22db8fed4ea8f1e378fb741a2da30c09abc817ef0ad589428f966b8bdf92fb32fefa67f425ef1c58a2d4c881e8cc0156519256b87736ed84691b585687971c01a23da37ab76726602a01e771d9820add53f62168d51c7ec12ede030b61155f12e7c8d5573985a9e0884bcc41e0b50aa066b3337f2ab8e3b1031a1c843700413ef03d3734536547fc636055f5d15b0f804b3d25f90e4aaf199ee666d90199d0781526bb2040144b5a638411c1067d0bc5e3ef1911c5223fb0f5d0ce6387702b6957f9466b1a8be6879b8c7190ec1e0662cda6005a363f5d3d4585e6d9d15349be797353179b0a713b925dddfbd05609430601339bf31226cce63c763d887364aa6f1d640892f2a074338b741b7c3303e62c6fc16f544ceeed251dbaed838cbc978eafbf1f837543876f611d64d354049936220af27edd4834df2740842f033821147619098b45b5819b49cccc772506d01ed8f4ee1877c3368e3863f4e60422ea8918758ebd8989b0e26041a906db0ade4365e7d322dad4f40b489b53fed41ed2d421c1571c7311d0357a511eff490193afe446f183baa147e12dcac9827391ef233396dce9465ab6d1e073a128cd062fa829dc3011e72d05f96956976120c973da24b874aa7dab15e6ae58272efc0ba97355b22d5ec283957b2dec2926bfc2f66059c0146ca91b53b0ffc03f3bf402c99ada7e18d713bf57351efe369ea204354343d3b080e8a386931ed541b7e3059f2bf1736b4903a25f0bba187f5553768586202d0cec388dabaaeae9b7ade100733139f77682ffab54409e14605ec50f5648da42702caa1d27bafc7c40c3b30b9ab2a54810e1e4584701b58b3bed1a4d820bb13308e9d5485c011cbb1cafd0021c0e888147666a3c00a59000f9195d6d9e423e11d7c4eb69d71541c7ad943601aaf1f7c6b5c0e058cbc989baa7f3a1927031e2219b398e2483a6b77a5d6e7e029c2ba36bdb882927cc06adc89b312bad1c5e72f9a27b48fc3a2c8ce431882c034a345c2692a323dc87380d62a1afdb73fa47d9b5ac0e495138e0dc7ee57835079ee2e9fae9b4e1148ef7df42e1fe213b99f573ca222430771088e05266db80092bf53d23c800b8f15b036883cfe58fac0682aca8d58d5ef2cb0c0bbf3c39560a347d51d3d1444b98b175af1abe341ece49c69b7022e61274a3d75f81871d780aef74495569cf97c7f4144f43a87b90cefdb6ca3e4ce8b12b168120499847000e67389d13f8cc190762f0c02ab0230e45ab1d53d59f665fdbe73c342169bede097c85f596595cce0d8d40a02ff89f81b64d7126c6bb73c3ff9739dc5af0444f01ab1818cc5f2c6c548873fde9095ca9efc2bb3f4c818fcb873ef23007a1253704ad561b872f774b6cf8dae848f98cb0af3d4b3c45acebccae094edc02b5250a0e259f8470ee88c12fc16014b5403fd3dbe29895aa411af2f1292ff20aa2fcf9038ee4453a51448cc7ff4a076d8c635619711ec70e8c43926f7d0df8f89ea156069bb64117f5be57b731dd3d573721e966216e98b71464342d95e712dfed27f50ecb291728f205428d335f3f4cd3e4657c2dfd66cceee48576f64d921e521f6d0d67df90d553e4c6751cdca6e425f1994fc9f1676a261f831ce5b0238c1a2ec10c6c94c5d22951c47b01c7e43b2e2199e0af412bfc025dc5d2d6ed6d154f154f0f9c7351c25d184ea619fdca59820448ee49a46e63ef871ebcb23af8abc58e9708bdb984519ae76303e661f0c00b44c22b6ec9bf95830fb047bc38106aad060607c6e1c4998b0cf07ba5b293cc96621a3151a67c7272e38f2e62f69528d1d19b060eb8c6f165d95dc13aa2e28b2844dc63bb3e24ac06eed20050251b698dd3f103e2f309501092c1b537656af79a0bbb2f398a2b0d8d7c976308db90dde5cc2b0a7ee510e902624a7c19f1169b6eb84773e131b6dade39030bb22555fa8ed9c904422269debded88e1c51b4294a458a080b78d4c76d72a49cfa988e96e3c3f900a86dcaf5953907318e9cf30b95fac95724df8982387a13f4f92cfa21226a7be0cf3d414afc4a7ff632bf803ff6061fdda12571de6c85477562da2e7cfdf70f9029210fd211239c5242002798598c278145b9501d3c41c12f998a7c82a6a8cdf06adf87910e52f43c79993edb24bf691d52a6ea175150cba134eb3fce090c3e401900e2671b4e5995408f22d9a7fa615301ac6153b69718b7e0079e3b31d3690075967bc173f6e716c2c30ee24863b96df0b1f7ec9c1fd7e7ec7d019bf4273ad0459d3c182ef8603f6b83d702f545cef5b1604f16b02657c94a3cc18323bca2708a8105bca731db97385576e84d78cd3f02e18144f2861bfe43590f5b203679007b199f0033dc73acc50950d2ef9f7678131f562599bdb4e91b97e673e30e7290eee0d5758dc3c1bd11c07df261222e095d182201524c271ee877a7afcefb40400197c1a153e4741edf9eb5602e856247f6845aadad9a3f3c1a3947099fc14890b02129d3382955faad6d4acc3186cb9749300d8ea9b32ec95ac0a8f11515e430b49b12829fa4e4e2e2754fd43313c544cb5b3521c3bdfc9fdfcfa9c35a4e79103a5c4f1b3b5e150f0f95951a4f9f7e43d6f2734b7ce212a1f14d70b2131c30009351dc1caeeac4c546240df2180dfb49dd0c780222d76caa4769fbe03fc826c055a409f05286a61f760ff281e095574472db33b803543f471f79bc711feb4a9052a4236308c714a2555b909047dee9421e3a9d04d4e7785077f71541a46084d0c72c83528bd367b260bf4baec560cab216f6a5ab82164bff19e6b0532005db300feb20a02e1eea6d9d31b4e2b0415295b9fe9c16650f25588ea94f0472328ac03bfd43876076e94c08506673db0b2fadcc91e1dfeb2439c9863e3063bdb833507bad8d987160ec288207c20a0df18b4c9b1b00b1d0feda3505c72e3c63f72c20ec41c2e4e377dce092671ea2f2ba7cb7b291d1ae230f8b26ad3b9ac0fbcf2c602f808c5729f1234dec272e59929673e32b3b4effeeb6fa74b33f211f70b80210670f0866ff75fc199e1ebfdaafee3468a818e1eb7fed5dae34520bf3e1b0c2105afce4636a482be6c9b51a4d27a170d22c6da8ae6324d6ea81f2bed486aab0f01b3eca20a60e739ebfab2cb6a374fa7287768b9635242ce1ae5dbef3879922c06817f3ecf8a08bb64f00c04a820a718748c714661d2f90b90113926a242c7bf092e54b99799098f8d4a10b52117f9b1cc044133a47eaf8b235d89db626407280766fe232f52ba22bc7c19113d43ffc4bfeff57b0dd811db81760403a11221c2056e9ded4f7e88f277158dc94ed1135d9196e61cc80e259381a7f3bc1135eca40fb91f963d6c38bf83d7b9b8dd275a146491a7fb0cb69539c3659f5360ede7170b49c57ea2d4ec258a8792cd2e7746c8955b79e9ac1e5ce0c0498952c3b91616011e98ebae173c7b74ca004c0fb2000b990a7a776f89b12204d02b3362778a67026e448e3c546bae75889435cda08570c72a52d7a30524185c21b5774c9727220408d5f57c713fd33aa2747ec5d61e7852a94db2b8b5d2ebbf90fd3a12b490830e7df75affa5d202f352b4e73128776b08a2fb415d1a354a3dc69c4845faedc007bb1856af1d47839959f012c44c0294505177938c6b4ba98939efcc69aa87fa029b68a2308e53d79dc013b557e19ebfda4bc93b4101c3596cf137c68cc7746702b53a6e2a2d557aacf8e7ef7df513bd85da45c1811529e4ab5e0bdc601f73b000338ca3699d1935b1a4d5a7e9fd36bcbbd9963d3a264c09d11fa76d1bdaa3480bc6230f56760cd25a47e0a384f1f3e921575e24f2cf9ab10d144822fab4e5210c83ca06824acbc8c4acdfe709cfa250185d935b38c4dc5d7e9b9bcd37a61cef3768a2961ed44c9ba5706b9b95e4fd24627fe34ca79483a2a04a483b5a69c297618dd52d9f107ceded42a02818d55f5a322df5d8c1a85591ef6a001357ecd8a25289e619626154939d5c02a3e1b82987411e648257958259d93b220ddc24aeaaabb6286f67ba87cb18bf546cd5008def5780f9af7d7b817fa2d30f9b2f4c667f1e63f9494e32945b4ca1fee45ef4d8fb7448550f5ded771ae79ec0ba48fb0a2b7ab4d7500e24ce807834e708c7afb9197cebba8755db2f06869b317374158c8f2d31c3d35ec57b0408331f5aa18da7bd620360b83b1ffbc47019ab09a0fa18d12511e22449a0ccfd24cccbd2edebb149f5b59bffe31bcd6ac5132d2c7bb804414165d17770f4eb8b8253d1cd393c4dc3d9216a538f31a4db9864157d0bdb900581dd3d85f45f36124c8f4ecf876d09cc626108b0769cab79357e6fe34ec1aeb94007a5416e8dcb13b0143db8da0fae95284b872c562f9f272e8d29eaaf09c8a4a3b1bd72892bb3ad3a4c4462969ccf431a40f3213940fa530c1adbb2d595e8ef94aee36de8ed09087d769cb8ad966404d7f689b60e020e1a977fbe70e57a31c096814d3f1b5e924d55ef79259599dadf4ff376d5d120c2d5aad9e14a92b8b656fe915a294aca796cbe6b15d4b0dec70aff9988a00514582afbb519f64ccc0f989c5fddfa9b2f50b37653e3aed60213339abfc6d0d58808d0b677e98248e0db2e64ac23b371ca2f9d2662a7c359d57ad770dc7ac4fd6f5652fd4e8dc66f8254969a37fbbfb4b2197ec864c18e1cf515f6b2646cf8fe5ed55427807a7aac9016cbe811c86f0b973916d5123dfb8e8aded328a464139ee92fd22c69d60fce6906b2174938a88b62fe96a1c8f977330c548ac479622b2c89db22099d5b1d1931c1bd15c49ad655d1371bcbed1c42734b3ffdcccf40b00b9be3e7cf6f5b8da625a450d1cd489360140022a468fed9be5bf8eeca7ad220ebca7e4de8ccc9d5ca2a8112d07a0014f8491bffa6968f6a70789c22a49ccaca63f196c063a29e69f03399cb7ff7f50bc572bb0902bf43d9c741202779c6e41b3e32c3082ef3a212450f0eabbf61e943d7d1a7dfd7a53c229e1c6e22a6d1893367b2b803ed23825888f799d73cc6471d16157f70c226fa39dcca4a08bb6fb3a512ff06b797f869007b39691f2cea01360fc5eb0925715f36cc246135279ee360522090e88ac7993cbd9581333c2a872a5755dd241af1e0fe04ee5857d16494222a32e4d9a1d07890492ce8320f164f0980ea133024f370e914fdb5b1ed02f174d0fda91c4a0f5cecb369745e5d7e4bf0fd4f0a2f55f5cdd5e5d20dd5076391c78e6f97e7c930f6bdacf40bd5a2edd56d3b68ed4027bba853f0342192a379999127dcd27c73fa21e06d37a6b6b5bc39e8d7c2189621f38e74a1fdad551586919d4193a3ea024160460ba1ecc2c9104e43a23bd24baeeb3adc43543d904d98c0560a246f7f21adc0fd21205dd4f2bbb3f4a9d16c4ecda3f558548caaece8dcb08e97d8f086e51d559505128753ccb7d2dffe54e3f7d5b63bfc9472105529f5c1ad027da671eaaf3d6045fe0b556e236a17d15b7d6a916a6ba91bf2e75045fcab0dda68803371c08b576563aa779a3c3f7b739c5c3e2cf6ece351e790633bbfd7055e50e93f6436aa7c4dccc986c6f0603ad20e9d810e1296b89a3270dac70d7d6cabe6bce51085895ddf4c2f6a8c63ce53c14fed78b3f89e30452a9478bd1195a9fb990c4f27c2d18f7bb54bd459549d6908255eac8437ca24a51a7b050714cd034961e851fb90efcb89a81e3212a0bef071ab6102906a9c0735f0d56f496011c39c25bb03e146f98121020f58ebc06da4537dd79e48b016561392b8047fd4047d9c5c0afe1babd8f32c6292fc36b1e79c0ae14c42ac2fee2cb108d40c6d91f264a3046485a3adaee2e23b9a34bd6f5b8c73667b7983ed1dfecbf927203d9afce3066bc7fa9ca4ae8d234192d637740f088d43b0296017bba1530c726c1c337c00ab288b97376a6a97ffb05e6596b8955c22c3501e373cbdbc54dfb1bf60367840c895d2cdca3513a1701c10e1d11e79d80bc098cfd9ea04d4296f4da2bd398f52ab76bea76730d82e76836aed863644fbe5c8a4f22aa7552de4d53e71d42e51b4f59851025cf3e4c1a8b57c2e026194f104b0865354683b09a13765451ce31198cc5e20ca54c1b7142bfedf03a7763d3eb0cc4a0ff71049a4556ab962caa0b3b8dd8bc78eb227e27280f789b728b416c2df52fb672628d7822e7757415003d1975634dff7ea0ec3bdb2bee4bca1de669fe68c3349a590cc9640c976b8d3580acb183ad2a9b9923e3ce7f2679d1656d921f4615c48466b28a8fcf309215eb1d6546084a7c53d837a59fe0e5fad510bc7691c29880284b696f387d5cd2214c08ed00f931a412f747354f98682f6c19e4c42e07d8a75729b8578a46a9d3a998d94b21a0c9fb235d18d9c172162341ea2c0533b267b692b670f6eca96e4087d4dbed18bd2cc7a7466527266210919dcaa8ae1e19549ce14a5e511e3d1f59836b4e09e59f016cdbae1b715030b9662bb7060f05ad0bbee62373d074a9b53f483732777c612cfd4c275fff73d8a501bdf7a0c2eb8494deb2195a59b6b0f99c50a8e99069d56ec104aca17f0ebcc880f76c6f285ce3d03c2f0be55d836f45f188c841c691d720f2d2365c0e23709e370efbc8eeb942bbdcbef23aeb118631187239b9bbcd70de0175e98b684ee6023b96702ebbf1ae3f5ca8a9dca6498afd0252c44930b1c90f9d3ba43f4aa6a75ea0191ed61e6e210ef71a2a7389c8c8f947ff668d85e2564bb07510f9a8ba1c58674a1910d0c4eafbe00574a8308b19df2f8b474ed5f81b080a1c9a07edb216f236b0219c5e4d5f24aedcb8f1807c55bc508f2cbc9a3e30458a0ca4e4ddf138897220d757c876c50f1fd14173e6358d338c922a4438809d606bc9d91816a634331ee025f65760f6650fc63c9c8a5326f0e1f0fb1916425dc108e684ff10d621b7ce10fc6633c7432db26bd713b532a7d9788db88c42f5c84c8f576b83b49b2bad9fb02c5a0740aac474154a6205270d67406e4b5d80af40896d696a0faebca9c910609d32b03f1b23ac722b1fc1801e5d0fe4778abd4f2662d025fe7905e9b38b3e0057d37c0bc65f9a6982fb2d3db32382860c2371ff463c9c81fb991703b548c9e0f777ac72ceec2149405126f9dcc5f18cba19012ace4f8e936e4324f30eb366d07726f143546efaaaa8309e027325b19e4f2b34481fe8edba09cfd67e48e201c0c251bf9218d0d2db8286692e1c558c59abf1b563ef17828830423164e642873080a20476073dfe969afbf47b04f69b44364de220a7212dde70ed18e5d50688e070190894c8bd3b3f06cd38280ec280849316bba88b6925bda2ba3531a8ff6100f6ec2124282045555a2131281abf07d2b36f2aaa4c60c98fbc5a72e716f66b00353c282443c1ce8bd23974be486e30269eb8e4583e62f1b9ed15d1bd180f5190e5b72ec9729b8d2c32285776e8a84aa32150bc08a12e0b09c3055181052f65c0d09844ed2bf6ec9158f648196acc5c5e740f6c7093436943ba181e60b0b525b0615db23ac58b1b67acbf1a0f59ea1a9f7ec9769e7ea0519aeb3ba1e8d783a2a05fe8a34bcd88749249e0d7ba8a0bcd1cdcaa7d836d2534585eecf38a1dcd5360d292e8a9c7c1c25c90a8fc4eefa3f22392809fcf6764c06ddb169b0058b505a08f7c38f4f8e8c12c888729c23dd9ae33df30917120150bd59720ce5dc169eed0edee1b0f9ecba4ad43ff6ee0e1103c784bcf1e4068a348111f0f4ed7397c8720fdb777c4206f8b70da082a2f8d2e92220229fa6c55402119861e671e8c9f31807b52d3380a8bcb1d29bc9795989fdaceb185839dc16cf8b115b3e0ba96eb4ae0b29762bd84ac30f8afe03f0ac7140515f28aaeedb50e98e51ee16236aa8410c0065c3813512229c2d0a96de8ed252074715386e5a5eba1b93d77cf344c9466e05ccfd6892160efd60a40af3f94ac81625acac363ccea77665eb6bfe33a69c570af107fd693722c0e9439c36ddd7d3a4a2edf496cafead3455cd23b69190fcd201c1db6315c8c03592c86a4ad3c17515eb30ebcdc42ed85099ad99aca3af87d601137cc4c899dd7b860bbcecb1e804d4ae784dedd01b3ed1e04d74e86160244c08619121bea73d7afd858287683958bde5319f5d1020700f52ffbe564a6e8e9109c1f8423ba825c221be5e9be8cb1d6b74827770c365c024b8989b18a45c612b0263d0a7c3cc19bc6c048c5a60fb228de2f43196a8802dc44eede70cff7d84cc0af37dc7829830e7d888136543b070d9a99472c1f70255bf70ddc9c0299fbeb007d92096ab2018d68144def74c223cf1d845a2c5b850613307ffe27eb2497ce80cde0d7c8243f3ac86f0459e3dbdfdae11a0168209e86bb90d0b47a5b91efa28093fe2ca554772cace53c08fac18d003be5d79afa78e7888057dd473761335bf007b0dc9cb3d79122c67fe5dfb6b3715a9412ec7b5b4b9dd95733d86a63442530f6a21f0b32249b40584cf3b3aacce380d5d3eae24f42fec00e6e9f73813526a07152cce67f8e46d89b3f6c74fb564d43a1ca94e9f04a3d406a635656ea020c4088f6102da9534922ba27d53281adf30b7336616060884c31c7f83491fe3ba5104341d2897549feb54172bf1ca72e6d95263e025c4698b1530076aca0089e59103e55ed49b46c356ca5458f2bc6789c2ca6b8062545dd830c5f71768f82ebaab03be78c77035c67a567689ddc3cc8726ee7811af7ee8c06e157d4c87a909c5840fa19359e07395b820876d40d80a7b4aff426b8e4e2803ab1fadd1b630e6c1db0de6809988ca5a56233d318f94204a126a605942d0628e7cba88ec227d4018db057b45a3ffff61be5e26c713a4362e7515a4e0b3d7f4132b6f5543e76c22a28d0eb4359c2aa0ac26651ac69385350cd5f308c84db05cc0ce991d31d04d18c6890d79826561017d38bb2ae636aca2eec453436d4510a4d52d571865ee4aea36c0070d167ce0035fb2aff4a7c450b60e0fc92d6e7405bb7b299d14537027b260340f1576bde7c27fdfcdd4bcb51c96e2272ebccc93fc834f3328668f27130dfae60976e6e17ce6cb4a4a2a108e58c539e90172c0109141883e0bb9dacc2b7df1810f84454f4e185538749a986ee94b34c2587d36813f06496b5c78a85a9fa9bc300e22d8f8055afae6504cf54ee99dfcefad61c4f314f1a9d831cffd498e066a9d0c389276cf18bf590091e78cefc8c657ac60e22ee8febc0acbccfbb6d0bc756d018fd57533905c01d33f684be06b6e8ac5d3164c2473f2d2bbc2cc6f33ae3d980ced83c2f9691090dde73ae3d94ccf5a02a32e5866f9457321b2c0c6b2932d100d288012fca3b8caf82080a7b254395bac134277699f0e91ccbe429df8504ae00d334e471b530642a542d470280b1157e9edd978be004bdbe1855148969f1b7607656fbeaa3dbf98da4103fee661c049f1a8cf0f722d2ca188a18614048514b90eb3988115b87b3b120363defec35214322fa1478530525cad835fbc075145ec089aa2a8efc02bfc11466e5355d955dcbf88fb74aabfdbaad82ca18ca8bc80a703169aaba1f8b1de6d036e5be656f8739f04f0172f91d0a3c3cff6d0e9f265dd0a0bdd5f9e954ee4534646adcbf4d157a90ec734dc4a0c13eb728135b29daaba02d5b2ac38b390502f181e756c69590f6c6bf2e6430cfa2e6d2f9675a219501b0c6a5006d1ea075a196e039fc9b4460a22ef2645bd98ddb11300c4d76ef0646c04725320885933e206a70c7f069408f5ef3c3bccc195e2f901cdfe81b7f01a870332aff32cf339f1bb2601ba45d062bb0fb1c1f182a55260b19c18aa65205fb409af3b27d5c85be5a508bf735a02853ad7269c63636cc3ccd0ce4935b500aa720737def7452e39b3235d0e115961de40c174063883becbbe895ac13fc6574df7045418ea8a8dbcf512172efcd5dbfb6688d949da143dcf082d879945901dfbb601406e263f616bed1e969964e000f74e9c8ca04fc8e4f064071daa34121d6b5c0b8886a328eb8b922bd1c4c511ad867f498ad96841ac7fa2280b6ae0be1c439d0dda808cd74cd5aa8698588d622f830a898beeadbda5d0ce5c71d39c479e9a0e0f5a481b848bff2c6668d80d6ef9668531d814d8caf51ab6c18eeb0fd688f0800e4416f24f4094e76cd19e9f083ee00ecf4275ee01e6f9073d4a621e1d98ee040ec72a8bfc1483478ea24dd45ea01ee8a5c516ea50970efb0e3f21bbd5737e6e09ae78488130cdb3e4fb52f54e3f644b45f2c88d35a28a1ec9b009afd3bb8c410bcc09f411680d7181084b8e4a58099512fab865f39673bd81901adccafbcdb409b43808c79551548b10d201cf8922fe5da8c2154d601007980aa7407489fac50cb5a734cde8580749968d53efcf066856d190543ff5e4bd8d02b66346cce24b0d9f33afafa9c899ae7145c1f7b5b0e85090dea52379a0121e6566903078982300fe9d03c7b57335ebfbaf31f52e720ace0c467703c27844084635060815cb3c0b986ed0b406cc389957548f04d70eb2a8e4daf0a49e9a54f707acb0a3c0f81d0780595f9a2e58f618693ce928ff54f2a4a47601c08532959c6aef0915b484fd0ed1eada9fcc2616e181d3de0b5e95c880d866bfff792e5d3e5549b80f705e94095927a1fae3a2038724b5de59a786dc8bc3d3286c5e7c114b9846345c004a6d05491f29f8fe23d6daec6ea5f89de526b0549686f8ea05a950e045fe1af95281097cdf8052039ae20553f0bb12a875b507b3c394515709acf465113c51a361cc0c9d8f621010abdb43e2b96f1b5fe05b507b70e80b9382e3129b0bea448dcec60d78eba7fe0db4bd25d96cb8a8a5faba3d0ee8963fdfbc0cdd7ad16f97c5709103f62e1b3b015ead01cc315291c3c2304b592e104fe8a008d54dc8fc15a7ac39087d63f055e7b87b9312d58cf3bf4858a2cf9a9319339888b680783cb902c9fb0a111ec094ca9f8ad33863bee0d0c243769b5fa8d57e81cf7077e256785fd3c703b4d1dcd47a004d8a0dbdc1ba9a76bb8631b29bd329b2742465b07862fe833308f67aee1667d3a7be0d67d912b997dc6118b9f4dd759dadf1a325f7cc534aa405a1641d10fc313e6fb0e57a5c2e2e603c5dee3896319fd1142ab0894a8904090f64817ace87388361e7a5b0e418a366308e7504d0347daa7daf90213b39c94b0bcfa9dc63d5b87daa690edf897b5c4aaccb614f267071853b7f6f92992fb22f0e42fa32681c44e181783cae74b54682689153d1bf358a8c3eca0eca65ff48780e7e760fdb82b1bdd4169c37b685d35c2f64b9f2a6b009f62324ba6da8bd4a7606d1940c5700a1a57229d82e6989414ae937f60dbf0a275ba10dda13b2c4502a01a5690fc2ea2ac5cdd445570e568a64f7a7871f9694eb3905a1bf0dae9ded1408024ad299d2f2d04bd6cab1e7beea5abc6e3cc7c87226d93f329007fec0099004da76d8ea1c8eedea6664f762a410e97c527ac8189ee310fd3136f5a6d552100b3f0d1572d84249d88a218e5e8b73df39a8a9fe14d900827a857f3aebef534c0c8bd3a42443826ed5c4ca4ba21591f080f6fdd304103cadaccc461638cf23b90cd8ccd1916603a6e9bdc086372e91bf5125e15bd837c0f9a6a691e6da704cf408dc8c88ee168eaba1d057648bb59d04d69c67df87cbd0142a6d5bd87fbf4d2d0f6fb0adb1aa1652dcca6a03788a4296abbb0d688daabcdd1963e7b98954f6be0ab1413978513504b9e5b2038438a1460cc3199068b916a7349f78881395e2fa08317db264572687c8891a3e6f41617d6448bba0bc17c116221d3542dc73f91b0ab490218da61eeae4b62b5f389bbc2d8252c2114db65dbed68a0405e0d153af0a344b8b68fa2f054402fba9c2ec5b6ce536be91b1929fd4ba3d8e20a2d4d7c1007ff87f7df5b4c057ee42c371188b63d9a1e9d1e63cbd296bdbff51c204cbe800c342558408a4434d554af4030855ddd0ef0295417c669615a7635e0d4a3f08024077d4bcfed78faa15c805cadfad0d0bb1f628dbc424a1e7a945e8908438d2015e716c83cdf57f8d6065fa86182c80577213f07437d9bde259a0e54414af410b6516de6eeda634d241486db5966a4f4aeb9d18e92dc9fd9f4fbd9a4a7ae15d0ddfcb8f4f6a5bb544f045217c36345fe43b64aa0c5d08ba1104fd13b57d070806e7d8a83731fee819f5e8f5a892ccdb64fde073476966d0d8ff13123b3d24040db42fbee63630b0a86858cfa4231844bc6176f36e6fcbd94fa014c321b5a05d0dead8501cf0015adc4e933bac66696e6bc92ef1345ffbb823207df4869b5d720e6fbcab98627a54501421da7a00ceb02973fe2daca9f78d41ac28c0bde44b2b0905733b8424df52ffb0e67b2d67e0fccec31f6a5a682dbbe42886c621d2da8c0cee5ec5c48a6f5400ceb2607b9d1094ad665100a84c35c5bb82eeb9ffa0695e0afb40b5e1605d58c8a50dbfb91a961b387f43b263eaacb365bec80b42615e33009ef7dd83787b0740e09ab385c991720b003377d97da4c3fe273bf7d35389980a19182e174a5b1cd8ffc83be319f819b6a5269c96bfc467bf3414d9260bc2e10b9a8fe95b539397ca8fedabe611c63db9130e8183a77bd346ff4520bc502cd323b5d10463dffab6b3278e73e5771ec7d2b103a66650333f58f04119036fbd34c42a021cfd12f2d68b19d1aa373e4bf6621dc2b7c42d0b27ce6fc01199ad5f34e74ec954dc8bd7342e573a58a8880ce28ca83801519e9dd7a3063db98f60bac11045815fd39fdaf1c1890bcb355cee46761fc6bc239c3584f1482054a9e345626f6a1b142fef1970da21a2be3f8e052e4e0f9a0f176099606d6e29dafb5a735fddd81cf462e40c424daadf72ec02f70e2aec1d18b7de4ef9d1ef9e364dea63e4a4080d720b6631268537be3b74c225782608fd2f0bf71c70fb84d88f0bdf2c0649a77e94cfddf710fee9d39e765746e09f3437c665c6c962a9043a798cb564f83e4fb5c99ad93ae89f0856c74a36c0fb7512770c91b83e40cf4960b128cc009e3aba2abf5685ea44453f4a2708f793503f4905423d4cce0a135f923ffc3176868ea7fd2cbb1c7801ecd483024123f61ed919f58f6faab196283417cc575741d348a98a31e9188768115a0eed01b8f47fa9b62a89e821c581cbebeb0069f2c7fa75ec25a8dbbbdabfa130b3443afeb03416671c872bf99ea7f87655304c5270e047a719716554ddd0142a529e6c49b67d41e78bed286e3137698313ab321321866f97f4ea898d23c3a37a941b19702530118fa752a06b7a1e4c61f1f3d9633b73b0c068ecd20e040ce4e0d2632c27dd41faf65144aeb30658a9171dbe7daabd36afddb03e4526e20d201ffd849c42cf3146b0860a0514367a579df4c0fd9dafea9949810711ed7be822d32fc901ae389693fe39e5b28750e510231457e09512d5f97733e99576a421cbf99a51ae3cbd7126423dff294b3b037f3a0aeeaba2ca6a8e1c58fc5b72f1f70415d0fe46c2871e73ab7a4f50a9dfba30c415c1e45af4dd48a7af8b0b0ba49409527e1b2835fe61e43610681e170d73681519efd0ef1775588ed5991b33f69fd378919193ceefe6e99b0fbc55fb2a0db84b054e1e1add6061a3fe18131257ad27e5af6338a7bf79e3497d966ac9906b935e539a4d9d94adc4302561c6725219ca4e6642227c6f331424750ae48189ca4d54f8cef0a328d6e51a0e4073bd745fa07bf77899578c200eac6e66d19245db9d6f8b8437fef9a7afbb69d1a99de13a5dce22c3f4fe2bff36f73e5243b1cee4bc155afa6301f3704c90e5d075fa079d178ee92691ce2152da8765b5f38f82b186d7bce1c853a81c7b8407e0906e88df7b754d58035c5d0536f7d6ade45fb47fd489a5a07ef58e9956e10308a399d2511fdb8f1548c341f7c1eb2ec632a54ea30019d256b595213a869b4502bf1d2806910ef2a31c2eb09fbca65598e1503d911f2ff0cf6c4d906cd421b71ef909d7ad6322a38ecf7d061c212dc1fd5106bbdb726e9007f78c94f2d628096502c5849f8b1c3e79a316fbb6951a39858b333e9d393b3c28d0de43b798c68cd8a245b1e0172c997dd5b8ae89da1154a60ead79f6b2c4151305cd417b05ed13703629de6212edc92322ca841febd3d6d0557d4e26fda4e0f6184ff06bc026f2c7f2b1023a4e30692682ad39ef70201c66bbe1d8bc39cc2f8a9363fee904e6aacc2eca41034791af9be167cf62d1a1c94b9ce32c9415527b43f5837505d93aff050fb8fe6ed47b96eaa81561e4ca29d6a02a04c9671d603576aa492b40c014369c9a9933ee07e3836502a3e7bf5ed99fb35f65e2e0ddc3dd441258cd4d9b161f1dfc53ae5699a3788e169eb585fd91276945dd41363044db68f86aa133349db78f34d54ecb163afe3379edc7dce02238e27d799cb0ba5e1b300235b6e8bc7e450484b07eef5c24954e4df7b75b04e73740e2403b3d37c73138765cbcdc5307c051dcd4466a7e21dbb0c6f3a30e07e89d057e6697c3fe848309d98a050bffb811d158301eb381c3ddc87af73831334db6f788f6d24afbcf83abf9c3a9b1714842f44329d74ff49d6ff5264a1b283ed91a9606919526a7515e105c7481e1468624fca6c6c1db4d389d87e196363421fef88338dcbf683b4540a484fcff7abcd6b5e5864454283c511be98f5fbd78180adf8a168f20aabce0667e3e672cd4de67b1d942538ef557218f1c4b9f45f53b20cf4ae1f001a56c230e363c8df87bb6d78c312be74f5da0b15bb18a6f219b9d52ccc3b1edf85958c492900204e9f13574a88550e2a771e588aff9e61a87d39c5929f6492a41bd289311c7d8740c4a91b6fa0c06432384476f068c0035a57c19c8730e5768bcfbfd9a5d6392f2bb4693990b19faab771bd6cad9e0f7cbec70ba3a9cbec9adcb821e709aa0ad74d921961179e33272ee98eef61d4755607517cc9db96e3bdaecf0c2aba155ebf03abb603c7913d6da5860bccc17ec6f072d2a95f51f41c2bd77e8d8bd997d68b624bc47037e259de8a72188cf1f86f2f36b56c92769998055c5dcfb0a352bbd4753369d3cf1a85ab820557f5f8e3fe76de3e5e6ed3dafcf66b4cd8a82375823d6ac2fdf3f0a10c8c03010d3309452b6a9b8bf4e3a7f47c01748b574d1257f0aa18e6c4732e2e69566101c0ea0b6437d2d64c28902c18351306ce6f1e62e7319e614113b2f426102ec80ca2978d8990943bae86d2077f35019cb83cc4156e12376733876447e0e91ac58000b93344e7409f3eea4afbe6309e23d18c0f1db8b41b6206edcb8c3cea650ce609a62d5647eb8320cc8a40d4ef9b2f900aad835174707f4ec0503ee701d280733d38bbc5e8ea2fa6716bbfa946a9add1b54e3471accb65c9c835cd6912b44dd343a077b0c7e14b89007b15d3192f811bff695929dfd824c1881718287d6e8b3e4a31573dd5b8b9d683e35144ea22ae99eb46fae356d97f6996d01b3353c06bb9b2323fe18f4c2b84d39a3732b38e9bc494cc43e07e64ddfc101fefc1aee9fcbcdc03b1d9120e7fc59633a6ca9f041b32cd43f4f37deaa30ca04542db96cf77cf08805361d04cf373d253ad4727d59968ddb4fa1b33474a6da08c39801a33725631aeef8196795b8c6aa6d56b60706666bc57b22849fd3fa500c27ddced7ea0334f9b92d590696a8270acfa03af69bff0f0b8268d374fa219d0cd224811c9454c83c533bc2719a08aa96be554a0f95fa4ca7c02894032245400b211a9eae98d96052a04fd9d7a8bb382b48468e6d5b0fc861e14ed137ceae560bc0cb0a7dc0af76a97b245966b22597162f4ec6c9f3221a4a69ef43fe19011e0aebcd2d760a352b3cfda10caf77ec902a8ae7bec3700899c9d726dba2df21f103bb703d885b7ca78ac746ca3e99831b4e2f7dd999ec44cc6922bea346618e6001f51f387c2cfaecdc740db9e44ecb4b0da370208ecf3eba1c15ea70bb06d63404025b000aada40e40e1e737966d7729c5084dc8e9c6392dcaec97b4dd6e1b8b004831345902ca2628784ad7ee435e803d31b5275931210b5ea4fcd5132fb8eb07d79fdaa8ce14a5ebbe21d7c2388841207a6920f04c6184fca780803a97930a0f5107450f0e50b530753f8a7efe8c9f298cc00b5b585e49217aa82da904ef33095421011f97c377aaf76f2390c6ba3405754e311e35810701f20c15ad15bd930c08f3224945681dd53e49094d286f89a0cf29bd90feac7a616ba6cb52f47f910a"};

    std::string tx_blob = xmreg::hex_to_tx_blob(tx_4b40_hex);

    vector<tx_info> mempool_txs_to_return;

    mempool_txs_to_return.push_back(tx_info{});

    mempool_txs_to_return.back().tx_blob = tx_blob;

    EXPECT_CALL(*mcore_ptr, get_mempool_txs(_, _))
            .WillOnce(DoAll(
                          SetArgReferee<0>(mempool_txs_to_return),
                          Return(true)));

    EXPECT_TRUE(bcs->read_mempool());

    xmreg::CurrentBlockchainStatus::mempool_txs_t mempool_txs;

    mempool_txs = bcs->get_mempool_txs();

    TX_FROM_HEX(tx_4b40_hex);

    EXPECT_EQ(get_transaction_hash(mempool_txs[0].second),
            tx_hash);
}


TEST_P(BCSTATUS_TEST, ReadMempoolFailure)
{
    vector<tx_info> mempool_txs_to_return;

    mempool_txs_to_return.push_back(tx_info{});
    mempool_txs_to_return.push_back(tx_info{});
    mempool_txs_to_return.push_back(tx_info{});

    EXPECT_CALL(*mcore_ptr, get_mempool_txs(_, _))
            .WillOnce(DoAll(
                          SetArgReferee<0>(mempool_txs_to_return),
                          Return(true)));

    EXPECT_FALSE(bcs->read_mempool());

    EXPECT_CALL(*mcore_ptr, get_mempool_txs(_, _))
            .WillOnce(DoAll(
                          SetArgReferee<0>(mempool_txs_to_return),
                          Return(false)));

    EXPECT_FALSE(bcs->read_mempool());
}

TEST_P(BCSTATUS_TEST, SearchIfPaymentMade)
{
    // test search_if_payment_made for testnet
    // and stagenet blockchains.

    if (GetParam() == 0) // if mainnet
    {
        EXPECT_TRUE(true);
        return;
    }

    vector<tx_info> mempool_txs_to_return;

    vector<string> txs_hex;

    string expected_payment_id_str;
    uint64_t desired_amount {100000000000}; // 0.1 xmr
    string expected_tx_hash_str;

    if (GetParam() == 2) // stagenet
    {
        // tx_hash 2ff79595683b546c8959e353b2c345394ea80694073d9237a146448bb65915dd
        txs_hex.emplace_back("02c2d20701ff86d20701f88ceccabdbf06027b0b71991727c05f0ec98bdc0e12fe598acf8eb808790c3692550a464b722ec22101d9d9ddd1f2b7010715195405742af8fa89e12c0d6c3f490c81a08e836e3d516300");

        // tx_hash 8a7e09ae4bc4d85153ff85350da82d076461db61e94b321fb727a0fcdd96e8e6
        // this tx should have 0.1 xmr for stagnet wallet 53mqDDKtVkib8inMa41HuNJG4tj9CcaNKGr6EVSbvhWGJdpDQCiNNYBUNF1oDb8BczU5aD68d3HNKXaEsPq8cvbQE2FBkTS
        // viewkey: 53c5850e895122574c53a4f952c726be3fe22bcd2b08f4bfed8946d887cc950b
        // payment_id8: 1f5d434f0b34448b
        txs_hex.emplace_back("020002020007bcaa04dc8d028f439601f51d57d302aea425427e03cb7243e20e512bc03a9c7cf2f3a3d6178e8b4e3cb4930945d710020007cd50b92b9af302a2c302d0599203ed0575b11353f8f35d282744185f5107da43f810c2e4dcaac815280860da016320af0200021d3890f92bb851512869bf2336f322e06501b5ea55b000030380833bd0cc51570002d1ff798caaaf37667cfa616e3bb009a5605ab03acf91fed4f4f2eb68c046a5412c0209018f19d83c6f305c75011137a812db81609018a33684ec8350d6f9d2219f235fc9263af85299e246cd0302c0caf0cc3b829bc083f46c4f70d3c5a7dc90908d665c29ad4d2be6ce07f7a1d4b9d55888cb6a76f8fbf6c5daf3d6e57d7233d037db92969f76f459fc4194279f17e359f664f986047e3b121dc9a5c0ae98d22abb96aecbdb14793c11249fda611edda9020b2048913a6a6b0a466ae005f199db5a20138eca1ff0e165f051ef91f4199cc10af8963b0b0d9e4b3dca91dc7e59acc9367ea99572f3b92f4f2718b2975ec6b00c40b0d4bed4589c357d3d785434bc94d4cc60a81a3ad481a9d31cc06f586f790a05fbc095a5d64d8d83d841d51834c5bf81bb91abe5fba2d95a871f6a00aee10601858567a1151d444fdfc43a37aa7a4957e9318e5b221a29d432ba3b4a7b9f5517ecfaedd04adef367e601b5deffae36ce58b06a677612b6d1328e29dd118f0a9a15e4e4b2d672962e7668455b76c62018cabd9a7325a7bb8f62cdbbdd19d109f9694587a5edad9531f2d69826e37882fc13e3bf77cbb76132622658bcaf1f085acfef452ef2f8597186242922a4cbe7bbcecbbd2d832787704564c2e4acb90f41f430abd50bc5f3f2491e3d5a80aaef5a1c9bd7305107cdd99e863dc9808106e8f33289308f51962820dee0662719f4580d2d506292f852516f63e546bdd20f4b33f2be65689e7d099199bb278e3c4de71cba3e067b6e3d3c31362ed505bc01087ae0bc13d793c56119aed7cecde0d16ecd5c85982d90ba0107bd9b5481f006fcb91eeac29745f8b4ed16e80a47df0dba161a6f63f692b3561c2efda7dc68030f0b8a19fbd8d8e7ee2e9985ec945fe19495159586dd076441449818c4dde40bf4fd7ecf3c94531cd061d8cc9af56b983d71120b92d254bc62755363c95ec70eebbc66de97fccf3554d1b92f7929beaab552e4a79cb14a0a0c27df3e2c0fcc044a0531d23c0e05a4935a7de35934ad7a6e50857629f165740dd78fb9fdf8b80694937a2e3949dd16655b2b449252c1eedd4a7ea71d92433e691d38afe7dfa701af904128da0f1c73e61fde12550777a17481cf19cff2b734d49e07510997a70d29bde78592c41e8096552edc844f75febabc80c31bb8904e35c0837645386d0632badc529963ac1bd865231a828cb78b460e3e68269a93845127ae1acd91fd0ec771694d0dffe7fde7863ea3342e5a89ec99cd0b16586524219edc497b65fd0ca6fbcc71c0fa632a38243fdd478bfe0c32e27d9af2d4a36e76b98e8ed770530bc4c70a2543e9c372e84774e82b5fde2f392033550bc9d82d5668d1b1da02c507b007e91d4eec5cb9e7ecf6ab2771c0d18d6f2d7d69747d1d649c58377151320138ca5e283bc244f972b463a2415850205ac8dd33c51178e04f8cc35d1f64fd079446d307d131eee099363b8a77a547e7abacd0597e96f7ef76b79bde0dfb4a014fe147688a9920952a9225248ee0c3fc4271d35936185b3508cd6af1258842087de145522eafd725a8dc9513ab95de2f8c1d9630052f363a7386653326a9df02e1b94e73e074a2543d50d43561868054feaeb999c9460eec6e1d6775bdc013096b67e06a94d90531ac7dd86c8e9573d8da74ebbba45d83fd544ba075adb69000896f2149b8ff594432655b92cd1fa61a0e2425c5b6370267e1929e9fb85d71086dd0923693ee0bc2b3bb76b47ee99703e31ef3f0989793a21f805605ea219201082790b3e37d7e646be945c80ebdd50b648463bd65055f342a66a63edf93c7075b6d28bae1f4579ff9c01a956fe0bba94b32dd941789110579233b51c4bf2500f24ff159a83f4a25651378c397a489fbc3e7b34439b5a5ccdc18182c29e21403fc4c1ce9a230854633887d6da03b20e647aeeb2c3f34d5f894c84295ffec7e0a1f355cf5d64d4c6ff29cc58717cfbcf4aa77e50f3c1307223b15b08401430c071ae7361e88b2137250ae0e29570a8a57acc79aedff9151104cf9d312e51bf2033f28024086377a06b4d8bfb2046ecae1265c727f82c2110b4916e3c1e342da0f4ac3df0a8b4b1ea2668d8b54d2b75ad7596544063d4d4d2ba7e52fdb04dc790860b8c39c75e9c38687ec54d0466ddfc61b9cec3f4a22173a9215fa60281b790b5ddd401de6a48bd6b3166607e7341729432527017b847b39efe9bf15f16d1e039f4ba936878408fed4be7b7dd147ae1ae07028c916711e020d8191438ebed308fc85b0e1969893270c0391e4e0bf988fb132b30c683b3111f04bec6567a9e30a1143edeecb0a552531b5062ceb11afe1db329d066d320690b9ac58971796e4050b93e15885ed31ed69296aa5a3c32ef6d6d68ad2e4ce039c7d4b40044df6550920ffc6a52e051588fdb16c7690acda11bee9554f25933a9afc994a4165cb7c0d33c232d702c6de7a2e7c0a6e12c69d4974477f750975a1561f41214720c9840ddddb15a20b3619ec6943cdcb569abba069dc499cf3b845a3e0c04d6504bb100d6c11ac99a6e61042bffebc157b3ff526e29012cc0feb0249360a35852f328f000d13a31baa0319d657ab11bb2e1e64aa9eb87fec7d93c3840b2cadee0412e20917e8615663e2d7cbb86c74ed382d4f43f2421578f686e3f3a286fa52f9906107466c2b92549a2c4cb47c95feabf7d7ecc7ccf612dcafeb98941593bb3213ef0a8d6fe1c3591dfe29a7a3ac12864c71ba042f823ac3ea5934a4d0363547985904ed39081921e3f4fe779e330793464be455b87eba4092ee1be77213043221f80b440fd19e1b74df32e2232fa8a998361dbc2239df1688394ebbe89f96ae917b0ccfc99e42fb1de180e1ace6482aabc955b11f59fa35d2d78c9588e5c7d66bb606fa1342fb8eae71c9aa96d37787e1d3c18fd1970b31e7f8313abf71f764f31e037c7b13c305cff3156624ba1080c89ffa52417dbca57c1fa0eccb45934e05290917b9412d740a0bef000fd404556326442318bb7b4ff349b43cf13c54e52d8905a84673a54a9ce248424b442563099eb6582ad3d1758556408d6eb1927d03b10f91ff20cf916d3e0fff26696621cad2a80d1f3b84a855315882f79e96e042bb0f01a9760ca9d2ac892e4658f1e8ff573b0f5cf6853e8e97628f6cc8e83585870610ab42ea8e0c667352f6cb3905f8617300d035e8c41ffb4d0539dbacca80ce0100940d631011d067b221fff433c2fb69a1e55e6c5f42e97b12ebd15fed057f0c205336cb6dffcad4137f22ce6353465bc10f75dfd952f85a00bc64b1b830fa0ea56fb9ffd50523850b4de936654ff72f5f0ec4de23422c34872b9ff0255c0d0fc2b26b78ebb49b457294e62a164f7b62f5609e687049a9594247b9ec593637059a5ff077f87a30c005b26364917bd5265737e30006ee39af5ec0dab7fb273405cd47341d115343b66109b59a6dee35cd17adbf73189740cee05efdb8bcf3740e12b9df9e99abe2888b33f6795c159a879c0d6ea876fd1287a1847ce55a206d05b9b5ff813b05e7c6c2801e4aca8d4f911798e07d50203d6f8e57a5e429667307a7d236e4a6cfa7c268decda3d729994b60d3f2e91fa764448b19930bbfa4d405e1dd1847dacf41f2f3a7b640a876d9b2ca9128970192d008f2140da255224d0afd8d7b1b9e4056fcfe8bc178bd14deb96e21870dd28ce5d7cd42ba83906216020dc048ab2c8563e4cf376a0dc816538ba429765ade6e9d70190fcd0b6fa370088264bb19decfc5884314eb0bc04ddfe022a952e9178411d142e4baaae89eda0e993b970d37d1a21336a7033353a780189a0f926bd5d87ddf8e531da370cd90045912380f52068bc41f5e1af825eee31817095d6038628aaa45f18dd8f5ce23020ff0b45013cf7f07f19753ccde985b6558e4da99d998974cf76b93a43cd409053fb61c7f6de39e7e9833f4ac9bfb790f06cc44140e7f3b0d0ade86512631c40bd731b9fddf36b192272a1a08f5cfa30efdd33c221eceea880068b94d8bc8e003da916fe949e660b9c00b2c1a08cdeb96ee9e8af8a36b3919f887562aec65cc04a12b1d3aa5324ce8de24f0bfafc46e7a16e81a1193fe2cda41e1116b17e70b07fee06608e771836dc7315c854ec1b5908b452c0963c204cbdb978c7dcc3ae009054645b48fe3261ee52dbdd779fd16cd7f5cd5a3beb0e2225b0afd81de907a0183817bce0afee5bdfaf6872c565dda7beccd2215e88d76270e98fb20bdaaa906ea0d9ca906e7628ca0b84dcc81915d62366388c456ca08db576a8ba97377c8017127ceb351dec4ce0829dc7873764c4e6db7d16c3fa60794d42ae484d24acb0e83989d26a428ea7866b39a68e347c49896dda113ba47af7e6f019f1389a9150bd4d89da91df193cdace104231129993d601b1c58180b3b5a7587f276c49d2009952a3b9d717809706320108ddc48fb464ba89764eea08fecb29a0ea30520c40f71d7c94020101c02af91bfc412f5c312a2050e4fc14263717beaa0c436091d0c2ef82071d835aee9b853e7d41c6e59c353aef0356e74dbaf37e629793c9eaa04c249cd7492c42c81fc9583a23fa2d83e4c305ca21d5f26a660467610272ebd04c803e0eebf8a051dc999e64bd9ca62586a26382665680968c642cda0b5caba0517a988f68f23973044b740fd02c3f4aa86e88785cc360ce764f250d40cf358052f3807c80252b825d7e46b7e1a161557b937ad0e717a479369c765856c78be06032f7bfdfaaff23b292bad9d4b32abec0bf1dac3799b9ea2957b736655ff9e0e375fc7bceb517c66fdc3c753555c34cde5a47ab1fea47583ad5dd501e95cb901cd63df005ce674fc99e82ab2e3bb7e8848c55bce93633663c21a42f2c7061c0ccfd7deb8f372aa0723a64d76350c4f6f2aeb0ecacef9bfb907246863a54103061af18b725b2cc7fdf52712ea0bdef1dae751da8b3ebfcb6152757beb4c8d7b0483da93c74fe645f4c701115d2080da9154035501b7e5249aea6b476dc0dd210d03088c2b2ccdb2a3dfee60d6b66e5326f35a779e10962c1764128b7e2fe4e00a7d650a3b807b747207eca23b75fbd955f12fae9a2e17d2d48af4cba2b84ac9040fda3d9b82615413eb1b5147806e69413c034c277fb42246eac4ef94b07caa0bf68bf1761d12622ed272dc68e7aff30c71114b6e309122a0e60d6ccdb9bad0065eb4946bbab3f35330e3ee530043313b4199164ce396a97e28a1d2a615be8804f1d8d1df6f4d11c260e339721e0c488e5c43151b6651f8c8231124359bfc770ffe5fc10d252890ee3f0451e3e22af5a552364f65d2e93151463df286b329c407a8dafe4c9a485caed267ae7c0e49675e7f89a3617128479ef4ee92e1e0aad103b5aa8db1ac51d8aba265bbd77225641fbe292f3321e4b8ad242640be6de2df08716d28a0bf07c7c46e6ab153c46e7ad2c0ad92349bff408bd4821003514438018db7f25477815e5da9808e50bdf8bf68554684ee27a16e23ba246d8d7c55c0049d1fbd8e8252b451d6e9fb095758e0424f6ac5fde5327de5193f330778028a0858322f8357a1f113b631da75cd3ed16247759b592202e59753f6ebf78cca720f0251f65eba74ad74521d2f1f1b9a5c701c39d7a6dbe76a00e8f314f0414c280141073217894f633b64df413bfd870f8cdc58f5cdaf0181d04b3246b843a0760391ded069e2b683488d13be5260ca6c73f825aa66e9d019447bac62105b30bf0cef1a65ef22d5cb752bf53e56cb7b92f2fcf1d3157046e30fccd2ff0ddb06b708eb73362ca0fe1438f83a9ba65b5b74373f026a8c4e7ae5fca42e6138f8d55e089d6f4378296457569cca31658cceff69f4ce81b69d0f3ac557b410eb2e14d7030fed86945c71800d366e8222ad3c11512abbc1fe62e96f9fc4043a335b658906dc78c918395b2c19af75753b15f7bcd5d99817da81d8865ce506619225e1d703f169e18ca5c2f83165756ecbc0077b6c9b0f1478f5d8eb1b272e88f454745c051e9d97c48ce727dc015accb2eda5ec51b737905ef387d65730ce4bdccf3ed8015349ba34f59acce4bd6fb3268fa98ae6f995c057e7ea24e4bd77b33bbd25e90559a76c5238cc95ff8e3a939a790b14da70044912a7a8590d77cd038ea62dee0d6f44ab51ed0f9589d2b62b365be5185a17adaba3ba95ff291a4885515ad77d0a35bde89471b5c913f56c00dfc78903781807ca8bd931f1ecb37643fb1237e900dbd5613b4b2f80e33c4250144e06d54e33e84dba5a13b6b6a6b7a5c610e70902ee946b8f2e89fd3e178226acb1294d7df9be4465fad38d8f05cf69d94989f753c32722a021129bf2021a0e0939fb4be3f53600d359e1f6a152b8da86d174015bdfd1b49102cc4365fed182c8e0e96b79ac40425e4dde9051637a0d7a5d847d67c8ed4802e340e06767bb0e463044bd2a48a47dddadd62de68a6c54f2ac7938a8e5a8f6b541fb27c2f174b2ccf5247d751b44c3c1c6364f8faa2efc01acffdf415a94b0a685f8ef801c2a770164b93a1a4ed00570569addbd27dc5ab9857e78bfc0fcb2a54f68c8ae06dd6ecddfbc78808706b2bed03211ab231d7e397ef9301bc5f0d411b329f44a2d5687137949cf972fc2d8420b9a5cb645638aec41670bfe5e15847de2a4bfe8ef16e021315aa6fc8f7041a1fe4100401a42c17a5a1cb1c60f3fd753017e6f86afcad28252f5c736cd95f9ddbbcb254c2ff16d2010271de0fc450506f3758ddff8be5f1b86e287c608786840cd979e44fb9f3a105c5b0942290accbc3ea2a09b7f4e81ef8379154a0737328e78ed18026f2fdcd34e649127fbe3b0d27ece5a42d7f3f8096bf1599341419a3b7c7d48d77ef60e3aa2f5fb9bc2fc318ca5367b1472c5497b0c96406692d7fb0b56830b90b6172a32fb21d183a3d60ff284745bb97b5d9ec2da024b0a68fa580e60437dc5653cbb872506dc795b48920101bf7175c03de74b763f93efa1b41d4a6917d3375b3caf04102152dbba3b3eb62f8ca20f7bc28dd82918020bd70f9f696d66c6823947daa6e6e54538cf4ecff2e451ec84e2c704113410bffccabf3828784c3bc93bb84fc593fd4e138e05856d64d9c5a2b7f35480d64c24fd795439210db693423b823e2b611d403d11a215217073e51782ff192fd412a6df3e3d59120e9573ea29fa962b3344cf340bbe4a9a44860f79634da05ae04b15377fe95ee6cca88ce2867891aafdaa45c37c6fc7edbfb4fddadb5bcd3ebcf6258c83e9c47fb4bfe9502f66611f2d4cd5b1825346ea457a12d2e748446dcfe2969cb37aea6639f93e142f48dc5c294936610e69b7388d157275999fa491191cba3d5d3a18af4d1548883b214cb5fa497a79f3b2d2458cc585a09284e25fa4849500feaf067016e5583f43c98af9fb8a3753c165e5b7e249946fb35f68511e5cb3fc26c5a61471b6e4ff68406a8464040cb184082671070db3807b4b9186d1b19a572dc55a18c886a03c6d763a6fe210bbd51adce32aaaf3d5d0cfa026b585188e0996d9e523901909299fa7ce595a16bf56b3217cf3153326d4df695233eb5bbed3f7fb6287cd5eb2b9c9f0044296608aa7c62c8a4197e999ffa7ebd97641645153118021307f6f21cb1803feeb9a3d352df99a1c965dd977661dbc094770a7cb47ca8ea1d26845ad4237a447fd4115b066aa7deb4d8009d9e20cb03edfceb72a7edff47178892379729986fcca268dc5ccd7aded2aed05c3103323fb7e9fff272ce63c2522851a5aaf1a9431739a81aa819b7a4a5b56c2b559d1973a2ab4507da3735819150075fc577540b858d48945381a1e62678154ea3a79a4624b506ec43660a55565580a13af9004a6768220eef6abc95372eaa486f991f1d64c6176aeaccc03d8249fcc62225289d363ac65890548d27542aafe3fc7a703405f4c918a6998185bc2e2b65ccee3f982aaa3f36793e1cf6b8185d548dd9ad01a41b76280249fe59bfd6fe3d2dfc93924b37cb753d1d8b7e242537bfb45ba5c1d003ee11ca5523c405fd784cd881f71b7ab69a51f43a3d3b0e999aceda02c94a4ce3f3796738716e16363c7d17cce4513f1ad911a0d6bf4f8ea97332b2061cfb1d9060543ee6eb45d72961ef558510b7981ed1b9bb292bde389b27961e1bf7fe580ebae08e188781d57671a380f5080300e5f5428d96ceb661f870a3262f6271210e254acdde2ecf309cac7240c0a6aa3adbc760a579dcedd93fbee761eb8c6e657904793926164be29eb519ef16b1d4e9d5814ed1108c41db4cf4d30fa6c440b75b9e9798d6af14bcda1d650469de345de93ffe5bbee419f87221faed6facac9d0cddf187dc1a35176677424415a95907f82b918daf923d62647c9d42a8c945899e7890f9599fdc4efc91f0d06a53bd3167a79b8d6dd73b3c0698b212e5c2a975f408ff06f98b5b8f79451ddb37890a11416bc66a7a3d72ca86d2f5270c6dda9cfea6d01818d32149774f9a663f3429373b75fbad429fe90e39140b7e03736461906ab125727e899a3ec2f37b7b73f531c8e686cf43f0663b31839cce45a9ae007d1cb0087a939385a291eaf67d538248f5e5981ebbdfc4d1eb61e618772c9190bbe3bb8c1994a07e71219cd28703d9ccc5c368b54fb4a00a28139f8149426e366ef26df0a20d7c94e894d9e09d069d13171b16047d1d57d3421d132371fb01452aef1b0761668946d2cb9448a09856c5d947d0726fd755640b25cb94f2ac169d1b2c9a5e1b6d56c7ed4bc883a4f820366c37a1c9bbeb8fa3519d18b4431d796a95463136725ce24f921da14e585b1780bb02eaa8c4c4b70680b10df3aca1c3f8000bfd33bdb855fd067e05f9e07860c43914cc1671d3a81b02b0b98db66fd1c1226f1bdc6647f86216147f0e2fc9e162bcc8b99eac1fb6b54f86b521b6747df59c77e169812cd474155ee9d98c34d417c3a672f3593e051ec8d28a4dfd7f9fdab073d3b86010f8aa92ef02bde328522a1b81ded4abb3ad37dfc63ed979017748af4833ce1d3a8ad2daa02a8e7b2258c2cb006ef77c9f5a1ba19f6f959887813ac127e86880c3525526f5e4355dedf7b65ab12a756a4870b6895c26d571c7b2121f43000c70cce210452b99ec8ced3ae900e09c7470c979b1e7edbaf7383c661309431be06d0878b983ae7302621084c68a2d4525faa1ea7e159295b7cfbabdef4715a12da43b6516dff1141f1a39498233f321c0800b8513f48a3c7be461f7581e6c5a301815f3dbf1e2068cb503df35ca971220f50bb4f00e244786f76a8831587b4b967bc3299d33c46259457d39337f9769a09d6a6f2a4085bbb40ad0323aae504a61904aa6a2073763f350b4dc3c823f83a0502e4369e82374c19434bdf1a7ff28d1b50cd8c278924b7eb73177f5fcc003708456a28eed06d54a29ba31295ce4cd790b046dd0ea1be5e5b50b1ce1decf81302836092d6c5d0ca8e4ba1e1d03cc0842be4d3faf93bd92979122e54eeb70b4c02ad44c41929acb7fa97a0d04307664d4e0dbe78a4760cece61f90fec6b7a72a08750e583941a1758e7924e378ecc395b1ead71372541c686d5e7026bec9ca4600bd2bf60e6a27db9ae70524b33351fdb572491584ce0e64eff79de91e4635d30daba9256de0b0e2731be344f573bc4fe72f69172005335e33d9f897d098de3d01ad5761331d5e89dffbccc916071f67c12d3a5ba6fa403119496b71398d380509221ceb68a03c8539cbca40fe39c17ad52432e9d53c28b40872760ca995e5830b6e8db1c7ec40d9ef61ce4aba9bb0ddde7f3ff57774734e086e603f9f181a7d0339ff832a1ce05a68a6aaa6a8e3ca241dd8f4fb0c4d501556d5edd1d395f74a0fe8d3156986cb932ee16d8cd148769082aee795738c4199fca3ccae356438070d167c020d89a6117d7f1074343d46dbbe1668d2f2a7f437166d3e4f7ce51927085ff14cd1cccc547153faa889144aba3aae615e30600b70ec24f6d4d85fd98b0403ad173dd0c836296a78026d6697f258a30ef8c92bc01d504ff0a632ceb3a6044e3fb5a30a38a08c329acffb4fe786e392b942cc2f5856968336eb014ea5410afedacff6061fab229f22966423f6d5dec640cac3665680ed109cf4dcc670dd02778e9ae6694ef27666ffed9dbe490237ab35e1762e1375a16960c5259703710e4f81874a0b16ca89c6bd7b778b2216f44bc21a78bf085161a195244d25fbc5014cfb8536727cef04336ec886bc72317dd8534c5f7111e669d844e55c16d8a50f00b654e00e3c8f1a6e07c39fd5726da9394e9de51588a4360766dde1e1932f017fea5639858c3e36add3c3d5afef2a94e986519c830d70d08932e41c462971031fb614184336de510b33f3a6539aefb60e103c826285f26c404774d07669950d55d85a42f6c8d6f7fd8bd586a8ca00bbda2d83c05400a359c2c0e1e2736f5803d518eb1649d9ebb7e952a2d94658ad47c411792989ffce3e246ae69fc6a2fa0db731394e3714b198eb009a44ac492b16b7d3e1da4d06d86923d8063ad38f630d72f862dc76095dfb6ba360aca73d16f1cf9c626d5dd2312bc0b6b429915a3b0e5a356f9347081e285a4aed460f53f062701dd161d0ac28657289ac5fcd9ba8040fa9fad55638dfc744d9dd3a17b34591d7973d0846a5ee9371563a44b6126d0576ab4470f413c82e571a994372f3d73ee756a0bfe90cfc87b55ca7a5550a3300576121b1511def415128bcef46c3f798c1ea7d51636add3fe11e96933d1e1705f28db93f472c613487dd6aa16d6957ee954e720d957ca062ed056c2fb3167d03149017f76e727b5e34ffddda836e1d45d930aa6dcd77c87ca0057218ecaac90cec7c1678ad67db5e57156073c126109d1f90ae5e5966a7bb60d2b7e723d5330a35d9e2861ca56748a9421fedf3aff85dbe0a837395165cf696653d993f4f3909d10f8659c11c480a6f439d4cf1755853f9ac0383eb77126dd39b09c6b7c994050f590a384567c62a040d8834269cf611d4574939c9e1ebb45c7022b260ba380eb018085fb098a3c7660e9579ab57b43b8ee5d83b95f70b614d30b9b4d3ae3a0b72cc6cdbc4372af25fe506278d28880cba979f968fe35b201f11a03b0602450288578c404c5da9e717ca814100045652b9cce752a28608c1fd8dc99cd0b39d01fc1e4b7514b10eb5ae2bc5c5470ce53f1780bbe0d4f31d22f78f26451588cb03953cd4f5f1680e423c7378da63acde11220962a84ba838ccad9ad45134f65d0092c72f3f96a087ead116b12b0148558fb102923d2004c44201eee6a61935360c07b4ce1c5b0f8611faabcdd0ec0253806f5a913abfaa0306a609024a2b60c30f47eda8231ad74a3477b2eecde4213726a7b9e9285d730b1517775326ca286501716aed40534e2ad2e74bc25d87c4594b05c9a97802a9a0df7e3addcc3203d7005e5977cdbf3b1033ae953f2746f05d977a38f897228ed111e14cb3a66e3dac0113c42c135bb013d841557d43434f0bb3ef479e8a2efe1c0e36287203c4f5eb0eb7c3c1dc8a8e7c236e1aad20ca809ef99771d05bde2b01a7e47ed31b23c06702446b9b2b768049343ba2dee281972b35a2db0abdb388d1dd7fb881919eeafe028744a16471207d4f5b93f131579f0fffdff40758f7d0d7d4e0fc3a099e7cdc09f23b436b399b735881c345ac3057d88bbe2279b608d185cab5187ad196fb8806fbf0901d223d662bb2c1465e33f2808bca80bd5b12de312a2b0890a969b1d20d75f1349f9f11c1eaf93474794270f258532c1d6e14379921e8a4554a0873e10005a28d60dfd53d99965719578a6d38a4f96c0f2d21f75075ded782a249a9930ad1b6d01ff3c333b3bd1993ec0836b850343a1a83dbe1192f4e2c2276c0f3e504b37c86b75054d85abbb6b1f0adb9b858450a331723c84e3dfc7145cd46aaad089efa4f3ad0ef70c4dad5cbce4cb07a583cad4d2f82c947978219d3855ba3980b0e45c457cc649b5f87ba948a04586a15cb3fc4f75c3cce0083ad0b991567d30d38fac6901fbf5dc46490a8f072c70b9a6a5abf5b145eb66e2ae49d5b4e22cf091c91eeb2726d033d83c357e99268ad23f9c3bc383dd654d05af372affeba6e062427b5655753d0ce98d835fe95de51a04648dffd7110b9d4a3b5ee52d7f17b0b2c2ea7b242199cd392cb8cdcd4843399db3c8889c0fdb16b140f50a3a1199100f0b3b08a89d8aeb5035e771d57127f8a5068331b923fbb2cc0cac9c222409608abbb585474939cfdafde09110a65d876981d0ae16fc44ab68ce88558a0a927045b9f8c560102d0b8841a22583ba316c42d28b0ace0a59fa6de31d9c0024c7c0fa970a91b40e4a2e62dbdc8acae454142cce106ed588694410e4df071e187270b33db162cd3fcedd58d25e129973969a953fa78fd5f0cf3db6877e8de13249701b18d04c98abb6cb7f45b10871ed79437bf118d13f3b0433cc0ff0aa43982d30579da381c99985ad6c6c5c22da35f6998e6de6b42b9488de6bc5921e25399e70a7cf610b881ee8e5e98f15ecbfd4b4924e4c154b7457b8c735172037f00d54901190a2cb7d069cadeb0cdb912b9a49ce9d54a93b15f2ef96a7f2c6369c75e3b0a8fa5f386709078493b518356f6a0190d45dccc6fbc5cf40bc288e528b2cb2109c1b0c7d9ee8615c5380d9c60c83f36d8e5b574afa038a17aa57147080420770c1168b209ee0223b819fdd48d3992e89115b16acc4db7cb0f051a985ca2a42b0b7a86e6067c174fba43943ba31802247838a2d076add0bae0c2124603805bae03e7c7a8383bab684d6588a8456974d23b576d5f0dc0e9a0462a5cdb8018c81f0fe58eb52c12daab31afe9c624fe0be1cb0f4b3063c1a046bf31f424e30ef6ec0a1070309ae9ff72804c5912533a3f951b4252d7939787a6ba79d856cbc944010ef465b154aaa661a2a13d2b30f3479059cc9a4095b07ba62082f7a4f47f46570348125f7dfde08caed0c634edea19821eb2b0fb623aea633df68e0749e6023f0e8b1e7d8f7f97df6831eef1b29649bf5d9a4704e17f81843602c14f9283928f065cab0aadae2691bc36bf43e593f728ca2b58418a34622434db36e76af12e010a0d781e2ae29361308d2ac8e18590cadcbe8481e69cdddc977a527eabe1175e054f94c8733ab10e35900d01229f470a98538620f8b677cb94a4fa0dab031e5505801432c35591a11a208f1dfbcfd67e1a786ce4a9cb144fa2bab4efc315d2f00c6fdcad202f56a0b6c7e6a340d7f7169074aec0cef915d9745ad679df1b0e370a030cd79f61918b818336f409e24a61501e5f7ae6c6e79a2e7d933fb7f163fc0b60ba19f008903cd6722c5f90ea5aad73a891287dc9a2c0b6f7f47ccf7f3c850715cfbeb12f5cc1b07a7a57c7642767697c361757d82eef9767bca9064e06130c5e1545412725c94652996dfb9997aa7347c87e37ab10bcc32cb090350ba8c30bea9e08e38e44e1e2e624f3a7cbd6e949124f76166e59cca618d9f114a0b8ba0ae310eb687f80ed3953c102d5ad73a3d4a021b753bd28892576866f28ebc7aa0381f86babc92d0ed8032d771d45271182150c0e08e3baab2f5d00e071cc8a4b0b023a2f3aebe6f053a411ddfaee21f8634a96ddc2fd083386c5955a0734b59a07925b7397c3cccb2e2141baa256ce60af1cfdadcc09ea68a9aab511ee4b38630c553714a929da7384b4559f02e2a87416647f33cf7b71e325a64fa65748609d0946428a7b0749934719f3224e027a6299580cc954b8c1992c220d8d9b6278f607b4d923f2b3886c5d0395322179f71953d645ae2eceb1ad9da75f81654f42550f6f857cc78b97d834b26cdb0a3ac10b3f0e25524981e4464b85fbb82c925e1b0b3610df87b7ea23129dce57aba02bccb017042c062e3440155ae4f948dfdeb80bb5beddf97ce27d77acf16c10b58299d02ea4a7f5c9a274c907c37f17a9ab19065aeed5beb88b4ec1ab61f47f695f8d7c6b8a12d5a611350867fff445cc8d7005b88a87c93e5af6e8753c5017d852a34313d26e1b0057fca43d5048105a924901ad6c34277d3853bc4088aedabef85cba2ad40b9e9abb946e1e7333754fa3d30fea350f592e01eac1d68c10c767f770ecabfc902f0bac83339d6df3b24984350b5c1b5a7122e204cd5536250ac7ee8843660fcbf4a6650e7a0180108a9d3228019e630fd93e8b1b434ffccb52d63742954fde4baccea72c55cdd72d23e1e0d70528714cb64c22ed511e771b08699812cc62d377063b0400d7ec851494dddf10070fb1a8109f257158fad3ccac69b5c38066e1945329aa6a7ef96e3d6a760ddb05d63d289e1b770b4b67a83e4a61aad9a28b0a63fff0e9d31c4fb6674136c4e50923bb391ad19d867a84576f23510bd4c91539559334d2c13f891a2e527d64bb0c3928c8aeeef17ae2b5d25187d89b00bdffe88c4b9526559f493ccb1de3f0c30a1c7df9fd8762107ccb37f67ebb5660316252c96a306d758cbe3404f1448d6000177ecb57549bda96675e781516da6a42c8571cb078463f7cfedf0646b72d2106353b8d76fe773d54179965d2d7de4836a00212c18f35d7af6e0ad1478ac9880d984cafadf2da38b18d163ad7c53c5062310effe52819d3ba42109cf3f528b807c84f5f7e521eb8b7f2c14e8c59605f79a9c3a3a542ba294b6126215e5c13fd0b0f26a2f2b376fce089d7bea19a4426ef3c57cb059150ea8eb5d7049239651d00a1e08b91f1288dda04dfc3d06c52014edefa4e9b3b966ca00b7772f2a5b5230b6b02cc64fa00a7e3ef32df087ff0e82aa8e8d41849dc741d046658b0003bb60e0c5a744ab41246fcd162a6c9f7922ba0b65c22a3cbbdd1de9f1d9168cdbf9407dd4168fb994a7166aafa9c03c3c0545685b9ba187885d2e269bccd738f94640246c37ed29be4f313295dd7e23c6b5474383b7c51fd1575fc47e9f7e1d82c730c90b0033da11839ab2e5950d2c4ebff894df78cff65a6f3033140d905c483f10eb026b2a1af435b00e3617325fedf36f0b3dec55fe10a67993cfc3182fa921cb3236bfb1f8991ecca84bd087692ed316da7930e95d3966d01689600a01d5b5c485dd7ef666d2c74dff82329fb7f5fd73d43bc5b8134b7e5a64b709cc7d8563c73e6fe07cde4647222527c32ca517e5fe60652362640cb0a34f818226abda7a46d8d101db806d49267bd63184c42904839e26b6e92104e640a0bed26e50c3541ac15c15b961bcb88da75b77c12bc76db1a53a99303fffb8c081fa3110ef3198559c6e852ffe77cdedf1ee4caa7d471c03906513c48c2dafa361b0c062baeacd3ca1c6589381a62c066ff60d14d9951fbb490ade364d0a0381c82cef7418a3bc4737c1f34ff9a8d095ef406c13b0745f5a2c98bc8e4654ebdd3ce7c590e631f4334e343612c13cb322ed75db1ea6bdb26578fa948bc09ffe61b62541b30bf6da305d73b1e038d32b9eccea38eedc0c9c4772902554fb93c9cf30d2282d8bce1a0c9dd3c4d37cf98904c26f6dab35b69311c2a562e8416d42cb28fa3e617f5e1b4a73e321486995b766ea0b38204215c5b7932f028a39c53c72cc91b07ebe18bf3e14f15ffce237ce1fef65ace29ae561a9b871d0c02a93ad6ef0ed418fb214b7e801084209693b611c63b15cb280dfd7c3467b151eeec44847fbf7e46effe09545ef941b9aea6f60b4d010ec8a8f137d1906757ca33ebe888dcef783acb9fc901ffd24edd81e5df61c6f4f75b93fe8b1be411ddb4aadca7ab3e3c483ce8b14219f3e3972d5294e06f5c652c0caf75aab7dcc30f7e105d6e1bdd3b95fb048c3feb03535ac6d568766d058d3ec7f732defd3cbbe24ce1145e514d31056ba395da2a84df7cb60f1959a2b280b1b9d3852bef57cade1629cea826ae10ad4dcf3539be4b989ea4d27f726f9b9079c14ec0ad1c890c47a8deb4323f32fc41293470b8febc03b175e8f9a9552c5e7153bf031d8bca3b90af50c9bbcdc9747a4e504fb0ba55f104fae79b6e293abebe2f4057d4c2862bef8075c07ba2bd7ad1df34149be4dc927cbf07cde4d35c66e189b2f0297ad0081cdb03b766277728c8df8393e9c2cef5f44f8b7d9cd4a2c055fc82f8305267075d8fee46df492a171810a8b7be1073c3983c227be9c8e622a9ea016de3bd385f2671b913b69f588eceef02fa0724b3824ef147848d1de2ff9e28c4d79aa3f985ac826747d650e923e98df125510a6331466eecb82c161ca12c9ca2551ffe702ce5ab5419e782516e86518c07b03bbf7e2fd9dfa6e09bc1c66282629131ae60cb9c9f8ae199ecf91e22ea46bbc518453296a4810ff610e5d8582426a727609b567416503d4c130c93c503b16e1d4919d65b4a397b218b19b731423ea2b69a896f5208119fcc1d62191ed99a58d58c581ccbe94a88377edab7103a9652988a7b47435d10b0ee3215cfbfe5711570fcf9dc3c26325f469d8a5f5159730ae51c9e0f4664a2c0966d0e5026d124206ed2f426ac99a11033b4ea7f2323c16af5b8b086a38f200ef18c323f667608b542cd6a98a2925b12292a4f8ea034c4ecbcfc7362abd98eb2d0ab5f8390e365d2e5a71e2c592ec6bf07ad46b91d7845a1f637f85f73178ff5dfbc59222f590ddb4e0b3265f58888145d8fbcda5700b8d6d556409ec95180b177c5de991efaca727bfd012c98996003591d8ac25f709d659cb152387f4b9955b55cc413c33eb3561630396fe692189311fc780a2d3bd8b4d07a21320ce8c5d1630da8fe7f45afcd6938f5495b8c8e6a8c6cb231a0120ee444fb17e2c33bb0becaa4df613b75f38610278e18f97259d149ae92e7db8d45b538d58a17a0c26b465d721f6f17cd473a92f3f99580e6490f34428ffe4d958688ad418fa4eeaee5f808965d244f07beed1bb49dfb83fb87c3f482532c0ae51c307a479b44d0b2e30f0fd9868d8a05ee0e4a977479b7c35f124cfba549d135792e52d155194fd7ce0c35a6b4b48f0a62f665811d3ab5a17b376349425285f1a85d8e7cb97539a7ed0fbf49e3e7a1223c5457a0987f352059934f2891e17108839a17e98a867255d3c5779325b251083acf885be0e6b589a33eb3266b3561d4b0ea2e8ec2d48fcf88412274a16026923cff13edad65a64412170428949296b398f9a8f007ba40adc341cd5c6828ef649fc9a52542abccb1eb09e664b400896c2921c31e18b1d7b99fe808ee0ed94dca5f9054b74793c2d4b9bf7039b4e066b59c20b5ade03fec688e57dbbef94b7e9bf776a291cdfd922611f681ba3f43410e2aebf3d399aaa89ebed7316876355f67fd5e1d9422ad398e4db1832a8cc891f16eb26b2b6a51e437598b105d5131af454b2c86fc5f9f18e25e2466098c077c93c70484048f28a65591f12f1e524adc9b8865229a580b8e393bf65fe0c2f297e52a5a4b136c8210523e07b96e61e50af0e06599ab41e07dccedbb1cd3d5d9facbbd698d9a8db30f556082b5b13992ae1b24f4f5b6a36950cac804da688033a3836ea6caf2aa66855ad2e672259548cde5f0dc61ec9bd2f1cdc67847a992d5200e2d5dfcb368af6aa46fa964a93eabe7c2e70feee332c35b9ce4423529e0c839c279a9a3b4fb5bfb4f720df2f05c6f0f9893ca498ba8613044668d8f59aab12f9e61feafdaa04d9491c20394ad731a7029ee979549837b120c18e7f8f9e9355d9cd754baf5462006bad12f455d981b02289111c54bebb27214ba4d15290e80fbd5b6bc82675b22dbc992ff22ed55cd0041a3c250f43befd7f68474ba5cb91a858dbfd3cc8793b4aef3cd0bf21f642974f74587d8914f1884ad7a3c80ea0165e260388a897f3b10287d670000b2332b1c878fb5680340b8f6429c13779654151b88414d34df3c55d9384b63439d9db1df55ef662392d6af0a841a96d1133670c44321836ba762fc7a2779fddbc4354e49b88508c1310ad1429137d5d138aeb4ba901edd3c944e75ba3d9192910e9e0fe6ad7ddc6b690cfd09dd52419473091ddc2d03fae780cae62f7ea2e77981f38888dcf3db175e0c5e70f85fca3b653e398366ce4376f49c6c480aa44d180c09e54f25ee605b26078dbe9ff6721468308f99a24bd05e5bc36cce8489fa716055e83719d9b301700cdc2b489bdc3941bdc75b88e1c44d1084ae19b868088a0b4d47dd7c1e1cbb900b8975f6a3cbf1280993a73d63ba4ef038d8edf8eb668e60058a6b719f1983a6040a19586ec33eeb60b3e6d1af6ab48ffa8fa1fe1fb6cc8e2955009437529f610cfc131d0e55d759e12d33b3e8dac60ffc0dbd313a84bdb5d9969f75af34ea2c09d07325297079859737653d6ceb33afb7e0992721b72cbfab26fc39bdf8b0df02523ac0ddbf3db8669f875d223a1d130329df93b936e009069fa28c066770f806d11104b99126b50de024f00eedceb0863e9dc1a705b359cf8659ae996af4ba0c26e356ea107df59b39a808f74d120d9ff996d823b2e890fc69db0bb42d1f66005b8e2d6278fc4e6f5b4d35a3222beccc87601a3aa1ab9dd2777829fca294b20ba74bf5bd461747bc6b96b78c5f5c7d658006eecd00247e44005768ee09d733013e53f30e25c3743d2342d9c184197e38d984e19d9e2a31e3c5ebc0cb627b11051008c3cd60dd10e9212022edf36865b96600a1abc3ecf7d8f5671aa8f1fdfd0a6587e32653751da6a007549fa2bf0c95794ed6299ef3a7eab451c924200c3209d7aadc9043b708801e4d3ce6aabc6d0853f852bcbc437e949a774ac79c181e0ba32b131dd0e824fd8c588294d7f98e331bc3672394b3ac2612b6a1d7ea7e1604bfe9d15672c94e107a3f1586d8da293a185084e0f035558c7bd12189cdf86005b96ced8ae66878e84899c0fc29ae7b803d7e1d2f3526ead038a77f23948bdc09b000d7f6b310443edd5d37b30d85d8b511270e6b8ac53dbe484050cd47e859005ea9244d56b1c7f41cdf20ac1e8a9b932490198dba00c45c9854bb0442aa510887e17448b56cae24ff441626aacea8c6275196f07efa8097726f67850751820861de612dff5ed7ee16b5c5946cacdf8a4390c598e009f03588c194374b46b209bf1643503a7423067bf6b30f1496e20fd43cdc601bc083edded4d8a5ab9a0403f67fcae65b92e624a4685e75555a2914305df4d9d4f3d42df527dc0f0b45f409cd9ee3144be28e3b832b2ec557c6583f9e4101436f0579799ff1f775c9a61009f15df24599283f0ca61db450b53882b1c6af435fe60f2f068f89a4ac97e1440d");

        expected_tx_hash_str = "8a7e09ae4bc4d85153ff85350da82d076461db61e94b321fb727a0fcdd96e8e6";
        expected_payment_id_str = "1f5d434f0b34448b";

        // tx_hash ac715b386010bd95a506fde0ac0405aa8ad72080b1d7cef257b5b112d9ed84bb
        txs_hex.emplace_back("020003020029f7d30185029407bc10800fe119f60fc6019916a008a507b80730eb01e114b609df03ac0b4da40cb002b606f010d410a607b205aa06e915f7018d04d106f70af603b516dc088d18d5055d8e020cb104daa32b6bfb671a92c2f9296e5d09174b509a69ac2e67988533bbbe5d6bf298aa0200298a28a8529d4c870c920f8202992aa203ee0e950ebd0403980226be016cc202b118b114f802c40bf60d8408cb03d611bd07b204c9069203a806169c15bd14e704da14c20e2ce605f205d5038526c0b2a4909185084c7113de20415c3b7545e5d4f0cc5b5fd21cd1d297ad1286ed020029bb3cb315c70db922bd109c22b00af023e0372df701a218f108b902c906a017a201f10fbc1196028c0782088e11e915d203cb048b0adc14ad0d60de01b602ae0651a909c305bc03a511c607980bb7147d8e2a1f1755ef12ce28185821e6b19cb6023d535411bbb7abdced139c923bad020002e4b04f1a4a46e00442df6fbee88056ef5d34fb87d432e232dc3b166be05fdb280002e475a89f6073029a3f6ca09f301d5a88e522e229b0a3e7867b2aa331cb46c8d62101307f043802c7d6460fbf3aab1ad9bddeb417c7a5810382d0b282d2e3ffa860a702a0a7edbbb679a40685a9ae03993e86e96bf3352ef6c7b90137e93053d0ea6ed521b6c86877c467657c7120b66248f4f2cac2293b84310c37ecf409a7a1c7365819387b039a613855d36fa10780922eda8c96c031682e8d35657e209b461a1fa64cdb07cf3ea55659ae1d583cd70ef6944164ab2c35f9ec26addba6cdd6d43287a58bf3e0fb0da0a3784186315a8ea8d8402026cd186c37c794f6d9b4b97f50e6726eda7fca0840d1c4eba4bee47fb53988a6b12fa4fdc661d20446d157eae0ea42a2c69d08008394fb8714a97a9c98858f1f57e1c45530f8ecdebc88a62f8eaa986adcf0940f32e62f9336efcb8a134813c6d9ac94736fea3ae7fa9969bb2acb5697bb3614474a297d61a76e9d352852999229a2f735b8bbefd3fcbb680eec46761959be5c9a33f8e99da8cf4e62cdb05b888cc2cba76020a2ec2b9843ca32565d84b665d901c56eaece700357808d78937231e5433e54bbb943fb6c8159dd9445df6561740a5bc6a6f4344efc2055b2b5c803d9676d7e8630b14c292756761ed158ffc6bb0a5a5243275192efcadfa63fcbb10b0b7b268c438599d37d1bce2bc30558821b035b7f9fb2f70e0f84f53e8b3d822dc42407848db1ae8eca2aca2cedf1fea2ec066332934cf1344d9df0e39c9607c8fd87a78d640d88fd01fc381b22bbbd7945048764e2f891467343ced6671e106d95c357689d59d6212e46931327578b8bce0a52a7bba6272d059cf5384b36fe12b33a26ebf9205c8589e32fb2cf16144dcf00a664150964d1ba8594aa7aa096be927192f7c33f0d9a373d5089a63b86d42d03e41a0dbc08b9b43a3650b2fb3d09be3586a9f3bf954d83b30d06b4cfb50be600ad798ae6903a0230526a1a97e2fdec48eac2757be16911a8694cf793cc408a0e1bbb3c4e3847f456af2d7c02e7a23428656006a488784dc5737c443967c742057ea11da8386a9885473d424f4c269678b4acf1c0f930d168167948b782a166060a4304cc1788e4e04332f2a54467cc373733e6a10b6e392a7948728b77573105e6e130bc88574bd54dc4b44a11f0af6b0eb97e069f113615d75148483fe1ba0536478bb1d745b5b2fc55d0f5237bd2b1b51406242ce195bea8d6882ed8c3c00e2f96ce3479b1d205b803e5b61ec19e69d1a64a0733a109b57d33a9f6063549020615c8604aff59dbeea47e33f3ca7d54002098059bc16d00f4623104cc17b6025a0ae39b2ad8490b54486cde43f214e73e556b5c5449b83fbb18196cd5c5940bc0d13012deadec31c43480138219e65f16dc3c729c943cd60c52cef818f8a30f97fcf04da1eec3efc26c29bd1777265c0940ee617f09952e0422d1d29b8c930cb150bd726365f349eb605aaa646f8d8dbf40aff57a2143ed618e2bf290a41506b5dcbe1e92d49d9bc57eba245f8e407f1ad1639346cd9100c2efc6adad65a40452c75fa8710b33040285ae0a8e5f71ecc471a5b01dbb2a4b9e3f526e7199a808e0b52b08d04600c2b50d67479c677aaa4e736ec8148195ff5c1da47cdf421d0c2c21bd7bc5dbdbed8909600e6f2edfc114382f651ed6be82118f52b4b45633095b89b3ca5333a8d9496c09f672feeab27fb18f750ccd8e00b62e936686351e06f1b017fd1838d4339f8fb9b6db2474815e2ac0a55e745071374ebd328cc22a06e65c6351e87bdeaf9994879b83dbaed95adba0d7fcb18a586f4883f11cdd3503b18b7912ce56293fab4fc17e84efe2d420318feb3be1d15dd06df254047dbc0ea5a82ba611ed0b8b19d1ff9f93e87c9fc00ce0704e7bdbf44ef916c3b29c46022a318ecfa1c6691122c1cd4c2147620ce26bf22e6e7e1d90e3e65b6d9a12bb09cc5d0fc54df90e7b8693b3e641e692fbcfd27137c62f8f8ee4036e543377b5079832faa7a6e07babf92e66880766c5b8cd13a6875ee40af2de1bd99eb1992d0dc79cd62f9034902a442730b9a872e26fac5a9e25c7e469d16f1b2eef7863d9064a80763b846b8ac6f9e81a704e43101cc6868d42b7ba181f59f05a70ebd75d08a821ef222ee4f66149f956367094bad0b93a4ea4f77df5b6c699b3ace1728a076ed7d515bc6a1dc641ff26a668a7b4a20a3272ff72915ee0904639fa62a4ff0f3435194c79b53602bd0e6bb5b6d6a41567fe5fecf4f9db931f757ef4aea46008ec6a0de98e4449b61d3e9552ffbcee27225f335478938ee6cd35dc6a9164870d6d352d594a175bc6f414a558f38148f52354cd2b5ffb981211e00a64c54bed09a8a27736e045f6285f921d5a044a40c0758ea1a5209057fd7b68601d810aae05fe59aa8d01e7a0924e73b38155697f9fc0b4b1417fda0bc112db9c4c8de68906d3370f7f975c5b1aef087a8ff4110e9177f66b66a9cad98cfb1bb7bb08de2002db22c387cfd24ad01334ac5dd500428697487b8cc9b6ff866b2ca6ed7fb10703008146bf85081339c08e605fdba91acb5af2cace4475001b44006894e5dcd50f80d4a09ad103d3c52c1184b390d45a7d31f62531bbc7fd822750485012d8dd06d6fa9330e144aea89c0976d83e1a7cfde34bd919d70a5fbae681a46ff9c7c105efc1ea3f1676c938266ba7b809faf9fb5a56e35fdf67f70789bf472c5701bd060108eef0eadaee6fea0aa6ae1e2b982d8cd9b644579315654135575375dc1a0f604d91fe99679c67feca854d1325dd936f0d9c0cbd9b59a951a587c0f1956305238d5b651396aad1f4ffd400f315c7cd52a10065544457fb953a771c05a47403eac42adfa9ab828f85dc66543c7bfc0a935ce723758c9cab72f8c0eb6190b10a8c010b718c39abb3160206ae5c7e917dce8e8d83d458929774f5d8c3c085a900b5824ece3a22fdc80bb15f77102b1a8dbcfafd6d6a28691fd3b5a3dfe372ba0d746e10d8b4cde4b7d0f9e3ca4faae0aec05d32ae354a1d3577c886ab11384f0e69b7d93df02a6164915c5ce1bd27bc1ec6e659befe7db74f05b4f9c5bdea120dfa9a20e3b07fb5b4da6665b12e56ccc08a18464a53abc6be47ca1f5033af2e0385beb9561de6ac0a0a725d9d99bfa55d2c67505f254638b01f62757eea0e0504e10df37765711ba65af01d5d05ec27059f348d67b757e956294ff0c2dfdbb4061af8c16312628a42a6ab9dba172bdeb4d1dbd46e01c17278ef1c4f8dfee9fb0f6657e4e21ed0a430d60b0322ea4844268d6b06dcc448bd366e28a04e25b4ad00f5d1def87790872fa967af929717c450ff3d3fb4667e3151ea28a86ffe8fd80daa86344c55159d5a9a52e9dcb6c95f68b194f6457d361c7855182fc3b2142d0f2a81ebc33a0ba0cac35868f9aadc035232e67988bbe8b22732e7a3cfd4d76c08c190e05e1a6f8280099a79659a23669d90460a538576496aa825596988dd15036907c34b4168398be55f6ccdfc30e6838de2db137d902eacabde171af2c62a0a0c8e813f6de41c55d360cee0004e665e85a9ee695e3a34d6b0141f6bf8d8ad0e5df1cc74f13f5658606edb647d6bf373cb820c3e6a9349bdb0832b38c511c00d0f740d849da49d56b745d5fe6663b3b8f20888c95365a1cfccdc2ec865d8db0e7361011b21de2957a63c403fa70569d208b450ca268447831466fb212faf2a0e178694cc826657c24d0464c3eeae5eb2f599ac0be26be9a47a389b07b275aa08919c8a6bf2f1a254031a45510cda93bc6348fa77d01d43ac222514adf70ec60c3b12adcd1125b4ab6eb2d57f91a645c757db7f0efd223cedc14f20b450456a0b74547a2a1da2f0de01a351de8d12d83227ce7a6a2c231b6c72622c47c2b70c0e9d162711e201d433e0a955847e1dae98e6f0fabc6c3e18e476841165f1c5940ae373cf4799c5c2926080c0dbe4133b0c9eb14761d811b8d9d7f423f7d18fa10def0a6914581d004f36a90790035b8d8ebb83309684f397251a6bdc3eeaa3d20c33b93cf7b397cd970c1935be54fbef1948cf5cd2e27c5de94f5e6f3b23d19101d34263062749644fd2bb6f03465540c741da480046cd08f3df8205711398210786c77735fffd1ba946a8be591348cedb07f2cb577becc055c4a28cc460b86f0230c8ae692fae0b2ee551f0a00f2796f2324003e7c997e7f1b2f16a219804210032a5ea2aaddf7dcaac1b77583467fbfd84e00c68bfca1fe598f53e618849e302359244ddb12d9c6378ccff9f764795fc06527de8b286057399c80392ac17bb0419178812465c488e4d7fb73a1dff8f90fcb815f1eab00db8070cb82c8a23960bd15a76aab107fae549aa325880515a8487c44b0da94f00318f15314a24be3208aebf3e231aee064682905832503be36591d140885bbebac9bc9b8af9424db10732e0a981b5a67a40087065f0f511b4bda4be1efc9d18efc46142ef16e539b90a8e64e1c8d4c1b961d26fc6f9fc25a286bb5a8a3a6436d189682295aa2c03e5031cf9c5410925aff6def113d9b2df4e4fc7dee72bf81141cdf02f9b25f9b2370628b206fecd4336a48196501d0439ca730afe9a9551ccc3fa6555dab307e05a0f31f3c518a940e495cfb0e31a31493685de621bb871188cfa9e19e3efcf123d08ca19e71d59fddbfec20e5bc41ffe70144610545bad03c7b1b265ebaae60de70be466f264eb287fe6f9a96358f3c0ba034c734cf97c7627ed83b985389ccabd0305adc78aaa899c3795df9a9fb9d58918520896b5d3701b5de8e8cbf2716b64063dcc10735009b20d9b6d555fc8efdb26491a2e13353f7c2cda654411ae35b4050af1e0b3db88d199599c7413c06befd008ea0ed5478ef694bb2903c24790f600beb1bdc9faa4960f3865281f69a3dfa9492edf5cf00e349556fe5d6bebd4bd0b300bd585268dc1b779cda0b061d3bd032a98a0c2dd3911885272ffadd27e760f689630bd133ac13225e14f5b5d0a4146aa3d905d3b9533a4ff95324b2f955c03f3042938cc28d6d29b722cb6dbd3085a3b530934c5ea4fd9759eb2896cdb240868990ed643f1ad84fac95577af808c7c1b4ff0bb363e35b1989aaae955009606a9ca7cfea2362260bd029e39a1ee312688197f7afcf3f3101d618264a6733a0fc2b155a668089e60b98647927d718392cb1b74977f40f48ef741692ac28fa8016d40669c256015f2611e30fff9bb289e946faa9444df521b70ca59df12df380b3598a9bbfd1388545dbefe63a64e9b44d253639375d0c6a36d15f123fd2873090aa229f7d844b8ba2ddd14ef5ec019c95c76f877d485ee7ffad4908b89ae5100515b66c9d8ad83b6700df320c919e9482ea530f4d3242346fec50863c6167309ef1d9a2739bccb9d8a086f87bddc4227fc71980e1e4b3fdc7718f8e7830a8807e974e69f4bdb7b22d5d40dbb6530c2c0173128e7a01d7f8f5bded4894d8b7d0ee667b9c36cdea5e295d12c08cc42a23a229935e061aaaa469ab53f839d9524022a1e9dc7c14b63f1383d49cfad8855535ffad6799fe554913ec7d06955e8500b7cc18b0e93f5c400c5eae8c02f8c9d08a7756d5c037150e2d88cfc7016943d0b1707023dc4a25293c96aad8e3e3c4e1fe3cd71bcf7e217e7053c8f2f8f08460780c7b51e35d0df0e0cc38d263633b6c15350bf26735a13bc830f730b0b0890014cec437aa6410bd62fd471b29af708b9a56e3834926f4790d2f93cdc1d74e60de77051cdf4597d98e4093c14947136a100f7f7670765aeee72817dcd8e925d088392b778208b042e88e959c7294da675e25a54c36cb95cc93f4728d0c9dd5b0e4b685170a378158291adf5b7e1dbaa29a9fa3127310a0dc61df79dac0fdbbf0b9e990fcab701eb288668e3fb9519960d4b427104db5c85d3e400b8aee41d7208fa66ca999e326f853eb402b8b8f307f8da08e55efd23a6e73d2d9084d40d970b217f413d7c814a965ca571adb81edc50d314508bd539c93ba0f481c926adf706f1c98077448f62021cf37056da3803f720ce8e63cef1a0aefb54bbbf3de18204c4a58f182b10cb47c553bd0aec36981860f99a258c815d20b7147a8caa8f00040d167f3a6c4ea733b1971592c37e4f2eae0bf8f2c5b530cde39158266c62d10c797565a05c78d95e47c002d5e979dd835a736e5939fe036437f876d038ac140b9991b921aa4262b390a0e7ee0319cd6f659376be78328492ed2fc494f75a9804cc4cee6293361c4c2534932d304964bb9ca61519cf388cee14f223ab6e7d460b7d56f1b3869b0b5ada9f5b4fd1bfc5ec90e4148626a363f79adef09ac63c9e0a654d09cea3567848341f43ee484c99db6fb38bca3eb70dee7b635aa84e7756125631a93365f3dd0f6befc532677c64ad478fa8a1f36e565df8fb872047a99b0b824a181750f2e2b491899e2e16f4b06669f4c0bf882373ef57e9c4f0c37f137445b46307e8ea38f524686e8614cec84728f571d4623106d1232bc2e476a1c8ae4ae5a8b3817a2ae865353dc402b5abb14e55befe8eed8860e9bf389092af47e59f55e37d4f0e1bae85d0824860f4bc3ce83bceb4cc66fc54bdf766042f45745de361e8628e6e96a5787274340a981b3b533068d74a6e448c549a601a85d81f32fb7e856c8eccbe0c261b220545f4ad98d31f484f5ab0ce3273f72459608c36a61e69e09762055b8615a24bb2d77ff32487a8fe0b24e5729c8dd24174c6626f3d41729178aa78f2bfd51998bbe6215cef9a8d92675f101072df75ca37dba3522e6f1cdba9c041c26013b0d9646e3ac7613f94a0a62fb8b389b82010285352abc9d9d95585c13a2e760686c8082dbe1e04e944342aeb8a20eef391ec2936b44e88ffe3419c09353764d915c83a0fb641a72cce7a8bc9dcdbc4a178b96b36add2a5c26d8ac0d93bdc6d434970b0baa03bac19251ba2b4720365e6b4ac64d114f8e8be66ed383a075c867791942e288bac583071dd33ce175be9bd9bd20c824a9e8839b0448135d26a2d0a9510520ac8ea2a62bd7ccc5473e5886bde941722584e954c33f177ebe8253c59f140d9ddc0321cf4d8a71e22d3499b19969a8aa82e93df8d58ba9dae06b9e0ec054591e83c6f03786af08dfcfc8912ed7c46073e6f1dbc3b158b853b3b98eb5940fe6435304b03fa68096fa54095ff667a517d8a70da7075ca86cdb7a750b6afc91de476ec4146f0ed46da1218fbbde0a5ca696e0a983537a6cac6f2ff73257eb22ff6a0664907d802d002094e050722146cd2946c07eeed53055625b2eb27a09234ce1dc8bddb11d097ca4179ae3ee4a29bafdef4cbf6050a7021ef8df8e2e8b83c951d90a63aa34e11f359a72cadd0705f6557d387333589a291efa1569f7039a5e1c97e80fae1cc3f919651aefaf9eb6fb1a2998ffa7b60feeb1f94ec11304ccef07b44f7989cd170fc1ad74abea36f1b21024a0098625736589f6bd008c0464f55148e2278fd74c71057c089eee0527e63ff063ad7385bc90b3279d5e7f3dad55ddbf8dbe6d9162e37c4ef6d6b8db059f8c510199ec5fd3f02346015cbf5bd14f3d501dbb6003a6d520276f87ac4b62a70053d875076f638e3deeb64f0075f52f4d88e5ed9c0a62b84c4097cbf1c90426cc35db8b7c45c58cabe3b428c293a22b364ead70a0a0314df3bb8db1d170b2a7766982f9a2b56a1ec89f8d925e08ff68c1cb4033374e5d61ded70d3a1b1d0b299b964938fdcf49b61abc2ac107a32af41fb0a71c06af157780d85616fcf7e80661209ea5cd0e22c58b2b75af36ac219e82ef0bbbb0918323fa5380a815090252cae6b1fa39ce149ea1a51c2df4b5bf7022861b196854c6ef17adbf943ed8a00f21b1efb2cab9b1d37db2df20fbca33bdc734be6f0171a53ffcdad85fef7c9c77388d168557bfb83faa6c94bd2c7dfd0de568a45a6640b89e24d4e95c1ca540a2f555fdff5e1a5c9736ddf3f64e511a683313eafecfdabe836c795cbc187e28b2e9f3806f123e79b7a1ecfe5b6518d4b542429de9f05f285a6117f4f9651915061a561c2fc63398288aa2b753009899265a40e027cb2ac7f5c00da1823f53592d9c93115eaef544e7da00223705fcae160eb852d346dc2771e95cc665bf4db6108bdfb650edcc92aa264c9a3895f8a65c21c9e51d56d369e9117b1cc7a9bcc2c6a427c2a506c9bf05187e548f92e116200e7f6d03ddc884a883fac0162b4609c6347a5e2215cdb38782fc26ee7f59f783fa9605adbfdc71ff8cba989da9b58f3f1947f66ac842688eed0a49b6346c4c6ac550c887efd728db64bb65766c63047cdf20d9c2dbb435e2107216d4d79bd3c34a202fb7874cb0f56b4898dd1f91dc2ec4a9781d7587a698ebede1094c3ca266f09c48b0057533a79daac836f263fd5918dd6ec68c286224d2517833b9373a529af5d30f4123d531dc7e0a41b89be040edeec3d112e59c4236b91a5da21ea2a237c7d9a9254c88c4c99d6af672331ecb746ef3cc68a5ced30b917f32261b36a58490de12ca5c7ce9d0202f7a66740fe4a53a19b86e615e47ab0bba476e1fb7f8b8d30a71317e389f40a7558aad4a74fcb7d0b1de15a412ae873a62f9fab00f70411a51c87b3deef305ef04f6b058ae0d950c3e0113daa89d9a847123bf7d51a37abc6d71b08cdf7489179fdd552cc6411f29cda63a629d17f8db33eed15f82558b0943a48c302e1b752276d6bcdb6ad7108a7e2040c23dd1001a420121188e2a4b69598dc4e3449e4a337592240b9929a306bee911217427b8c6bab496b6b1daaec16b967ad81fba285daf161680bc0842eccc1f7d1d22b47346c0a75ce1486febbee003c152e410cd9fd0938831c034dd0e6cf0b6d55ee51d5800ea66b65267a32164fbbce13c3cb3dc22310a640081344daecd3c05f8f8a02414470a9d5be03113e09a74fb8e818843851ac6cf571e041351b8d3285344a260859e6f58167a6e10d1715729876d1f652e2cec02274b0ab3489aada3561457ff40bb502a4fc72500e5891b7e9945699ccc09a65661c4846bfbf5791a483c8d40ae89483f5d5243e37b29a4723423b864a6c84a1376048e1b5fe6d2e698b805ecd1fa503c24207877cc8b246a28c723414c35555d5aba7b0a2f4370547e896802ed3ef7f8b8af0dff86b17d5552c0e1787e24b480e7958445122eecb96b8e3eea0f04ed23150cf58aeed1e8283fbfc1e7daa68a88d10bce18168b99fd5475f4351e1e446153ca567d7a5bf03ed6243bf16fb6be20e2a789abd270ded84a2ae70890b437f6e72500a489bb24fa506c836e0c1f32e45c7e80885da02e316ab3ce1cc0d42740d58dd50661b259b902febfbe61b1ff174f2e3a5bb0e0595665f6aa68a371e853d4b37bda9b22dbffaf8061b3d8608656cdf540157850305f0d11c2f18e0914a94afc3d5e906c02c129f253dd3f5482f42c6fd8d83980b27b03e02e9f005d1b48dc906dc4c2251734296b2cd516603bd683e85bba3a90800f32bd67f7f435536dcc53ec13c441ca8870e3fe430c986bb2aed537ddcc00d04dd24d82ca7cb855b37aa4f50b00c6e96f752e6e51dbac8521779fb93f9a10d45eb0c282fbea6cb4f0873014d94570cbea65a3fca7cc7e5c33a39d2866ef7077ad32329960eb156a2fed72cf8b238742d6380cd8726638f71c7db1a6b696c0c133fb11094cd50badb3999f86845d3b80a87e475b6382e75c2f93e5df779a60e902069df32fe776a878dd11d2800330744f34a863148a2f583f62d4e0bd3a60d66a3c06f74b8c48dc1587f5065cda073065367bed27092a38e5392900cab6104365ee13030f7df7b7273a9c2769ce52263df17173cee27377f80a39c2999ea0b5b1ec4beb2e742da1eb775711c591a29ae49e91f38d79279aa05217831614104bdbc6c2474d66d3e2099f32f9d9e77f9a0ecc229bb28a94ca9f647f19379950902b162d7243eb807ab3b557984ca10ef7e075baee11fa68cad3670cbabcf7c0222af8cbc81ebbbf8af2eb2ee3068da8da1352b52900ccc761a0b01eddd6ec8014e11214bf8272d98c0a2c0183b25b1fa5229e3f35e2fafbec9a943fc40fadc0080e5bdc6cae200e6ab457fa6826c22883b9547fb080c5f1d81bb35c80d240a0924a3842ee67ef2458582a9119fa3e7de01dce510eb13b415dc96dd0074ea1b0c1b41cc7feb389087bba19baef57de681a4de914ea6f3e6ac66aeeb3df55cbb0c9962c51dd4ea6858d4cc5e8489a48643a04ea33f7b1501830e14d85dfff291026a227d9abf4409102cfc7dee479a5a0b6d9d51c6167b1976b8a4e0ede265bb05b1207c8882e80f8275a1ba8e56396590ca6a01cdd01f44af9523f02bbd281f0177b0f56151a89749f1f3eeb41f234251b32c35479828526593e4325d03a78e07513f45bab4d17578ccf1ba70142192e4c2c46182a573f2c644f8506ef151d503d0ab7dea33b93fe7b2820d6754c047ee68f0c6c3673abc5e1396cc78e5f6880ae47d590e4a6555ccd8b65081ed424f52181ed6ac2f2ea3f0fa974081a96fb90c7dc28a2b097b07cea5c552d0e8747fdb4d64ac1144bcf3e7356a4f1703efa5016c6debe2293995084c2e505aa8b2b0793e7213e1f33fe85c939727dde004390ef5a5c3f55a559a7a95c2b6baebac0936f54e84ef3c3ac7a5312017a60516d40a1139722067002d062dbe7f8a99f3d71009ebdeb7d3f61a71ce8d5c893749500ece9d8a599ae772b2b5963ca22cf64b1b3fb3818902ac197f1700490c1b18080eb904f244eec322edce1c28c2103596a111337ce879d7a8b9ff161a41bf552b0fac3f4731f570ab997a9e4c6fe07b12b4deb3e7ddfe80922b6e1ae67389840c0d6ce10276d3f28e58e8d99f3e2840e6984478a2f4f93b2f7c61cb18b9213ed2086491adfa666a2bc325f12e208893528f093cf141434ac0709bd0871d40fd0604fb0de1372aeecd5721c2717850a034310e513d2f3c3d8b2553f61adf691bee0feed236327b9a35d26bf488057acbcced717389edd899b4ecc6364e596672170cf671d8fcb84a413aabec33453d6c076586617d0b2ae3050373719850a8b1bb0425649057da1501b1de0a8157c6ca90129801eabb109f0441d76ebb3e66e8d8022e5e31d4d716c4fbe1e8896cda03cb176b66f550526c44513696e40f6995bd0805d1a6c82552c89905e60124f1ff62f399f5c13bfdd51b34bf423f4e47788e00208bbde55cfe84df84f556f5bd6134b6ec5cca7f343a4996430f83a40ff01d011ac84dc9dcd2d626b7bd36712a51874a5d4718910a8320f0a791e485fb430a0686a84700d62c0c1d718e73f58cd7e490350c27ef8b20d1f2defc176476729003c8899e51a94d44492e10ee58641577e155ecf9a03522a3a47f8f52da8c646c0c60b9ce8caaa4a9560eba2948f5626e450d1eeef5bbad97b5b3165a4f3308aa0b1c9d22712d5cf6031e4f723b08545437461bd1c86be1b7dcb4a9d8ea1151f307128f8ec81ddfca6d76814e1236c9c110fde25da3acc0a420d51f08556be738019cb774a71d0a17341c38ef6b19e6eaf99b0ec2c5e035b4bc25190fbf56862402b625ad21cf95dff8baa7cb6f3a2402f81bf5a09c069f8bd26f37373343cc6a03a54032fdc863372e65cd4522e2b87a10331232a992db2556cdf88c80af47c70c533d32dd5a31648f52d27f22f8013a73c2f3c23e9f31f965d206d981fa25f20ab516ed95cb675ae1e27e330fa9a5f4111ae22024b3cc7ae3a18783690c107304ae4ad44d3da76555e4300ab3bdccadbdbcd0b380229826d7a61be0e96650970370113b685d7e0fbe50604fddab56a7e8efcf76722eec26c1bcfcfb4dd258730287638195bad698d3c08bad704808c25d3fd7732a1e739f5017a822a32a07ed05989be3b4ce8a132e5b0754e8d0613fc175e0787f0cb3ac02aefe41bcae7e3706dbd234895f17f63b883c770e4fc3fe92e43757bdf8ed90c3e91c2718586c19063ebde4162610004bfd23efa9239c4adcfcbd099a4ea796cd7cfae35a82b5060e21f7ae31cf398b31eb8123c9fe4aa9dc955f8766ee3a1183f87b8e244e401d0df2f21abd3cc69626d79476f98d81f911c39d192b898ac60c84ab6aa05a68fa05820efeea91b295669a549a3092c735e9cd6e11931e31d94574460ae35adf1102c4650a25a4e10b5407540ab77d45f69a789c67b6790faf2df4f54f07baa24b0a1925a73d4e9b72bc2ba3c912fd2289f0678211632fc856300a97291333ee7602cf78dbfc92872bde15d1dd4edf26dd5f252810fa0fbcf6939eb796d1008421074dfbe611a7f882cecd3d9683b348ce404f170e33a48daa450c76c731c980240ab355901c39a18e4856046bebc9feb92f3893f4fbba03686c30f6a32b3cbcdf0253020a47924ecf80645451406595c471d1e3d1c131d205b9ec17c7c9d99bc701985e3db64b9edeee272bda24b4b7d26ba0b5e14961b6c0e44e03cbd8c3e303005d96dada7b40962dd7ec148f368f7768a2298b447b4a61d5ae63a55d10242a01d1bcee4d7f505a77519cf84706bbf9c510edbc7213516ef274c807fd2427350949f18449a22bbe53df4e17a505d59191552b49dd6bd6038ede55d20188e05300a2bf01c4c7b544a504d4f2a99a443da93a5d664539a92d9126e9a9bc03f2a3095bea9d39d3f2e015762be2e42730f5a31238272d1525ae93594abc6244ed2602a267c0d11e66d5a7e8506786a66b616d54654ad6060aef395e872b802c45860e858b53bcc468794840e27245bb34bf5c54de7aa8e741bba24eb34f702bd221022ba6e9038549d8bfe9850a2d08c596a1d35c215984c0fe99e5539f477a098801f3079e309b040c09c3f1582632187da7712c5b0d9dcff1db571f9d88117a1509f968fbd6b24df0df4b28d36793188447fc1d55fdb0e95a60cf325465142e9c0d1f3627954e1ab653c4f85a7800306a317367d237163dbe433ad65bca9dc1e10a9b8d3c256f33f45a57ed3238c820a9b912a4e13d7dbd29df63191406b098c60a8b2a734e66360eb00c1a01ddce703d079ffb28b9364810b85c5e9e8f2e56c60a2eea10bfb644bace93ff94aded2c88a55ffada465598d7751d5ea541072cf9012cf7837353c5546c53642eb62b36a232af9c48196528a91a34757b37d160970d88a3918a2eccc02ae830983c66b6da78ccf1d53e63bcbfd7b94843c098d39c06aa63615aa96fae494e122a53c2b8059508d9501929e271f21a4fb36c07694b0f1dd134abbc05061320faf13c5fb9adbc54773181f3ba91528b87467b844dfd02807d5630bf44a0260c23b6c4c7772685165fd27ebbc521c3b8c346fcc0fb1408d6ceda98a1eb3b37b7061652f8c37ca137d691111427917ab29362acabf7080d22729cd48d7c9cd0f4d8136e15bf8f417e57bd273708d8f1fc25d68e57e336082474028515f0b7621aae0f9591f55ba651baf806b3dc6c17dc87ddb47c67b208030a282881f473e954e15fb835a5e804694659a048dee1108a181f1bab14350ddffbbbd5211dc97a2e40b09744c2ed65c24baa62f0f84804ed68c7a64c2af604c4ed564b0b5222b885866dd36174e536351b429301c7542e9f8c522e5b60f70c8d7df51e2ac2b868d27dd6c5981317e6154726477495706da62406b5a08b2103fec6fa39cae90502cac07a9d1e0b7a36dac233bcd0cc746f8dd7e8177e9265005eb6894d55b7240f965c80eb1c59c1f4ef73e5c05439bcc43a16b4a8ca7e7c00c51ff57b2d0baf22f6c4570e5c42115950348e32ebd809506e5b71d35c0e4a09263417c57e0a0b4f62cd1f3b8ed09b45ae0ce0b7cb7eb734243ec42cc38675005fd791d4cc3de05fff7c7e940b86316e36419c58e5571121bfb23b1e8db2050c9138275f2dd7f30c162255efce299b4d2e1c7dbb84bdec13ebe4f43f2ce62e002dee6e64791ae20205875300af38b122b69d91ce88a1f2b609864849819e5303d7fca2f7969ee28dc29ab8f2de2e2eacfc4cdc7c99eedb73796a981fdaadfe000387aade4b9c37969f8d1953de540b8018e7e322e102357b8f822850b703c207d28540df397eecd8f2f85cbc97fe27a8764b6df194b8efaf0f009e2d3646000cbc000b6de20593d0ea33113eefc49604a7291d75b1e26758b975782fb6e62202d39b4a18a6e3e6ffd34fd8d15c6c36314476166007df07f487ff1da83950c402f545768e1b7d83c5affb8b2ff6292c848c863cac88edf757558ba415b1881806c482e94f7933ab7445258f28792b98b17dc0555b7f16f329ff2d70b8c12cee08b09fa264ef3ea5d0a6b0ad6e3afd17ee79fb82d9555c3a60eb2f8ff4464d1e0003d36cd5e017ba53feb4ff32d2cc6b87bcf06ffd247e74c267f1cccb6f988e016f510690f89456f0e037744318e7c809d3df7578beb00a175da9f7ad227dc50c98372aa338318034d49bfa0056f4e7fb89d865eb50e2092205805467f9713d0847566bb89f837efa0d6ac47d6b66891a4658894b3e66f4adc03a4df8447186019eb2995722563fcc7eb42bb6889d5a8009213c3c35bac6ceb3ad92257cb5d30ab3df50825d856b0db46701bb7b7b8e4dca9b399900eb38af2a97cbf9a3ee250a682db7e79cdeaadc8b63130511c317aabd994927a74de3421e6405ffb7c2320a9ecb7cbaf73d707693c6baf659afbbf964884d84b44b5518c0efb407e64fd50f8eda91cffc7878d998cb45da3c7c00af6386e848a343a9c3fb4628b984634f0b49b713f731ee5e47d737d432a62d6bd975f2238ca0b8a5b5c1d2d7bf56105e0089de39b687293fe7113b7480886bbfe6ea936493ec375290ce5032004545b407f615ab2b5d881ec3b80fd9fc0d9c5ac13889384cd7c8ece5cf03e954d2288e02e0f9f564daf6aab4459e091e2c1d72a72c83f3ca138337689738fff8856edf0f563141bb0c23bda83a8e0772c124392c39576fa51f15feb4b9dbcf9eafe36905a584137aca2b0c119b50d2fe7559e4a11120d010ad7065a9a8ef8c377bbde70dd978b5f2b7df1630c086bc3520a46b155f0f64359d70fdc1795093f0cf302d0fb94b894ebd5a58cd88c743196d01342834687b1fc45f3cdc4d37c45f01da5455fae5adc4ae6ba60be69b0fda9965a14bb5b1ab8304ec37e493c696c76212ca81e1320c8f44300cd31ec329df67e10c8975dfce15c9e0c0f6edf37c4b8015cecdf34cd6b1c81e3989888eb508945607f1d52f9884154090f2db79e7efea42aa6e35d8997f97d161b2c4a71b2d6ddb262fcbe48b5323e21657c1e1c3a3de3a99e5a6f2cd2b07b5581de751772a0962dce4f5d1665492b6d8a1a8054f2f5c09a1f516c9c4ecdca25a2d94ffee46d02fb6fae2be3d4490c7f99b3a98cf486b85e214f449754bd97fc50010e0fdefbbdee85b1ae67c5cfbcf28c6d12511814884587d9e11115b077326c8c047ad6177ecc244699888e84c3bf6146a7f06495f160481f314c9c491c534f207dadcb503e8ceb14d0ba134e3d98bd751522020a8f7a1e0e7ccc03d738f39b4ba89203a9d3a9940178b934d6488b706afc44f98b1a991bd154f3f70d4fdb376feab9987bf4374c89ec1b517b8600a5b04a60a41537f2690968e451055c5fea7175c1c548ae1eee3e47e055c84f450f58ea655c7149e2c8afb9f4ca066d6b17d498ed6c03f4c5619f540215e102bf2270d0e4064c6c568548dd61978437a6cf88dd764cd569a253a15c6ed59fecd94706bad8c22f75aab763c3ebf66d03cc33dcbdfed027fdd3919def67e03f11763f7b24afd8a4554c2d5858aa5040f00442bc03ce797e02da70cf0b2760a45d33da87cf8fbedca958f919ef89e3a91803a39cf9136235d6edc42e9d8cdd60ff2f46523fe6e3ae5cbd39f186b38aaf7e1426ab29f2f7e6d4d97c2cf03bcffc435cc898e61603029317b9b3b4a69af4ff8165ebe7362b3a18862be2da18b30a84ae7da4ea2284f61705f495215d94332ca10d777e730a8d40f22b2a107d912098298bded2045e88d5a18e1f1be2e22d585024ba75478debe99541723d5c0d443e86b716cb1b3a45dfc8ad0af881ef4ce268ad8d82664cfe35462b8e95f7e7f4df5819b4c77728f5a41823005c436df8d50fe541e99f238b252e9a16c9b759ed7fefd0880ccd6f66ee4de2517930de8388828e6fff34e134d27f13681cd7997455c572379dac279cfa43c518d42b8eea2a938e0d84aa796d4d603833e179790f40f1614fa4fc3f5d5a3f71ba7e534112a282f338facd23b5ae93d323071698b02e3803aa6fda2805867f08d612bcc2fc62f5a95e53a38c8ec3d19fbf149c2f6aac733a3886d0fc18fb6e128446765e5a63b49feb9af9339c897d7b5f654fba4aab38785cc63e31267ad0567f8ae203860f81a96de0e4e295a8497bebbc1ddb02fce003810f4f7a5d75602fea3f2f7aa7b7acadfba7055532774e715a7f637475e0e9f7a89a73b99bfb0c0c33626b218e3d8e8f5ba7f90be42164e6431f29eb55c992bdfec24fda11fcd25ea4f36b7ea338b2b5fda4dbd6897c42af5b38b131afd9257e31e708de5cbaf09421e35edf698475e5fd8fe9ae4bb310ffcf06c905105fb934ad45bcfb11a824cdd81336748579c666c0ee0678933ac657b2238b18138bc6536692e97e68a483a25401d2788a46ad4eb835de1b85469132c2ef46f199f395cd337dcdb59fc980da3f11c5859a1e9d90951cabbae79ce49d6599ece7eaf29b2bd6dd32250bb07fc44e1dcae345e29c5f79137affee02bf2327a937712278444d92c8539ba47f992bdf96a0f5146cbfc5ff8d8c1f0be380bab2a16d13f440994eaa6a07b68add7abed1fbb1276205542f2dd2a239b1d142a41d2611c528a7e5e9391829d6457238a2e37c9d5fb064738350d4db736e5cec5fe63dbacb8774b4dbf50c3e460f3238b7c0053d375c8b74a9a596ac503385fd663c0b9739f4ad86174354c0a8f332ebe206b57a8c6c13d98b82f0936c3e5106c78676da1315a304f09f355049042db73215ee196330514d74fdc4757c7ab85ddec1d0bd370530655dc28eabad424633887a7cef88cea66a08c1156113367a63b3aabc11aeff50dd83cc6223b9ec1156245f40cb80a5aee94c35d0467f37701a5d9ea91384f7af8384dc72f377e99e0d4c9f71b0d8c283b78d4fde6aef9b1a2e2a077d3db4932f948f24f94dd26c7c34dd50d7dcece1bb1712af4f7180efb78115e2de6d918d75f469f25d00baa234927b1e5460fc0a4c68f0ac1e6b16f45795dafc4e2c54756cd9e4d2759b279af2c4d1dc3fa0ac5b800d2f4fca7539f1ac066e49318f0896845184b17752584e78801ebed6681b51702b87c8f39426a54b1f10bbe4ebdf09aea279f8d197db0848cfae4716fb7cc04836e8bef94ad22c0738f2d54a10e9c60633bb876217f34c46dbd0d4b024a571515b394c615f92b9c10765c55aa9bcc37689d8129dca019fb9ad8691d451548ce17b8b3857caf9f8a9a369dbb758e7f04ba21faf47439a9e0f665148cc24240775e376de3d2963396609a3b1ddcc0278f7ecf50ebac30a3f2bed9ed31d5c00bf1b8f293c9dabecdd9abeba4323089ce0f248e355a50de7e8ca4cc6b5b541919621373e9ac1515bef2977b7348624930a3610a78161cf794e7aa8ac8f2f1cfa49a5cb563214db74302e2a019df33855b1f84c6f625d4bc865bd2492167fa48dedd96c99b5dbf641f83bff7c6809ae5a57efd47f69bafca9def65e08dd5992e969f1e33c316d4a7ec974a49e81ecf91d153876f855c5d15dd041366ec1ce0c5a6ddfee1164579da1e54f1c1b50536f3766371d140b5562bc4cb6e121bf42fa074a052a6d443c33a3a1e3cb71cf69d6bb792ffe1c67bce6e1169496268f057c3950b8522f25baca2e2fd323eca4938555ae6a7eee6b1e8b9a49ab2d2c23aed6c9f87d5cef49132d97bf058d3fe26d14ff3583fb295b49f1259eec0a60d0312066825e01a9cc0d6da0feea25244d9836e4740eddfab6702209794ada3094cf9d95dcee85fc7d9d4d6488e9654cc32b6e299c77298faa602e784711ebb0ebf26a40de3ba05451f1dbf068948926844e1f688d5e8e5a0dfd84f76d4d9ea0b9ef0917c8146c0e573096afdb032520e68a07f500e6a82154a232c7621856f0f96dbbd0cea6043d802fb6f612247cd2298034c4cfa3a9d50eacf2a46c9635a021cfaca32c6e3e04aa8102b0c9160ab9a86626d2a0a3e4ec9862bb0bfb134a10bac73f31bfd7dae1f64daf8e104c8ddf4e7ba91ac5a17f0d329b431135c8580011b1a9abfee16348d92cef116901572f066df756f4eb3cd8e165af38781c0f803f7dbb19d21a58dca53bffcdd4e99082d94f7c9dfa63d70e13b821e0ccc101f0ba73a734589cc29eb18c7643d60926a0e188494e7f8ea22f9035b489c077f870a82f0e38daaa253a3cba327a19622423d9a0ab52dfecd07554fff6f168594ff0b055dce3d487f092fdf2299da15b675ae23d596a653d801f6747f9ec9171864052e684838051f3130bab825920afd6e08ced3b79196686a7994574fdf3a6a7207b8a25149a6ce620552256fd3152418c5497ea4d0be909d5a3caf2729349ac2071d9fcfc79adb408a72e60e426e99fa63787699d2239982e36d8ff3542744ab048372a60fd1abef461c5b9e123917b53750082effb35676ff45472ffed047c30d69885b8f92fbcd71fd9931f4369d0e64edd844685da117382ecd782efa775402b14a28f8ed73d968b088449bc688e7d3cdf4a1350ebd55689ffb369d6e09ac089e53f629ad2fd33ca44e84169d2db8b5929f1488dc78f0e809e70239df52560f7714b7735cf72c7efe4380e61e83240cbad834d0c5c3e5d8e5666e852fbfc50e04c70210af47cdc1fb1c1e6339d9d30bb2bdaa3113f4ab31bacd33e237e83e004f1c0e55ab80caf7c169941bab4ee0d6cd209ea09582b73da3a26efdd3d88c06ef679311a0abb5489f21621068c6965e2aecfcedbb8302a3f34622e4f3e3e306e92fb476633f050c9f27114c5dd0007b83543cd2f73b28463fd3cf10762f8c0c3dcfb0acc1aa31afe84f88452f60f5a1347c82152ff7a3b6c90c6e37144ff10b5bf7f940c055ae4232204f2532300a74e38dcfc97b8c8d81bccc70b289d09a0a4cba2638e0057c25c300ac75cf425f525aba84544da6a438201c987546410f0c319c86b42431c2d6ad1376e26d34e1d49d95dd5725a6cf77f5250ed40ce3af0a5cce4124316ae70b5164f7de71b277008a5a713a8a548e0ee643698f4e5d230356149f40447c7ee59fada4f30eb3c701977411679a9a79e45fd7c32d6cb1b80a4b8c773c656d5e84ea49d773934a6a7d29159fe9d9cdcb4793d39906c64a62080c0f9780b28277156adc0df257c065db8db4a8997396962de24b06abf8b3e702522a9a23cc77adc8812ce705795ee6b36b8bf0b15c00564176903aaf442b750c473d83cb37891d47c06652e47699641e5e0b5422e13bdf7889de64c9f6e32208837ac7e499fe725bf0624e038f6f1dbb12d763bcadbb718feeacb6af62bbb80b854822e5fbdcb0115c3d8ff947dedc7ea338fa5b0b28e7f24ab89524bf217c0a6f433814b9ecc4d4ee97acc15e75bdad6e06440869bbdb279ac987f2ca78530e3f45b9fd3ed2a334942857efadcf24b222187dafb400681cbba7088c8a819f03d8140ffb290640dc3274e06ff8379472b67a3ec0d15e62bb0bc2d60799dcbb020e80b40ea0ff2116d875bf825cb4cd04c841e2d1ab903d13b6b8979d85141d0f602794e72a7df0cf630004cbc2a6af5df49ca352508834df16f300a1ad0e2307202170f84c148789467052f9eaa16600445053bce480a2db3c29f1f864f14f099081e038ea978945916eab57eeae180b036d561aad9c2f4b797b4b520d395b0bf581081efbc62693ca2d6046fc47243c74eba9556ebd739c95fd47b8ffc64a01372f0f4d36722fd8338b3cdc9ed00887e2337b1ea8274d9039455c4190ab720b8c6bd3c23bfd1528af0fd912548ef6b3962bc80284f51e42a5764ac150130500e16bbecddf38b621741189791bd756ec65606817f52819d9e68408d55c724704ba7670d03fa19042c2b5ca2dfbeba236151e8ed9ab5b402ed0145a0ee9ecd60b1d992ac6c89b7889a85605a9c624c22021d596ec8f9e9fe107e0c38376504105b2d06337a06c95165ad5fafa4dff0ce2833912df82659ae743cd864582fa1e01384bf391a5902fb2a953c04ff2f9d3303d4749fc47ef422fdbaef16c188bc2062dd89458b16964d37fc70ca8a95b9268af361f33f9170b1e8abc457412eaf20b8823256a61f32faeebf6a5609f2598b676427f1d4701fe58849b29f17beef90855c8117ecc9d8be67b3a30c5b0a74613d5080eef993c012fb2b03f4c1712e0046bec5b21c7d9dea5947c2913396c068597f14a252ab4d37fa177a351e37bba02ed1bb5cbdca3d98277ef53fc83806a12ff1fccfe4ec522393f4ddccab619150ecce92dbbf25c474d083315297cb1a4cf92343736afd97ce2f68c49ff3a3c6f01fec45fb26e8b5d6fd88d0575123fa44cb3348abe804534ae7f84548ca224f707a0800b700ea5c46663ac44110869bf0ae98957a87387b8e4ad2147fcac4fad0e10dcc354daa871f1d5b0321091189a2f557a4511c4a37343e2017f8018596707d86ce39bc920e6370ca820b642f1abf3c45f5120ce85043b13a3801ee3f7c50e1d7f08ca186d0a0d43007121291cc729e926dce41c418de8922509a46f04a408ba59745fe4278f4b32de5cae4ca87da2d6c262e1142c0e035151150e7dfb880b709498d673bb88bc1a399046c162001c75cf066e6f07398dfe2e55c7417fe40a42bb0eb79cdc2838f3b49579063a6f3ae911d9e8d9f705d2cebf67d5df06760bd844ca3b9a6296348869280dab06fcdb965c14e7dfb08272670e7179de0f7f0c8f75cedef4993183010a65b39c337fdfb8f7a1a83e103c7dfdeb3ecc0ad9140332238a1adef9d8a0d0d25bbe7a7089fa62299de12b951aaf7ccc094dd9a1f5055328a7fbabd0ce73232c203d3a00eddae99d392066d42d563c58d14b79e47b02ffcbb4713c29f298c04813485a203fbd39f52b94e562fc9c20ab34b530e52f06aa16ba805b7ee540b096a03892b4dbfc3d959bffbae4b534df4cea33d1dfe80e54d805078569f659d38841dcf039d3c5f6c425d0f7fe12340ce7bc17b8d35a04e73d302457a8a0bb0793a1e5896ad2070d8f5b0d2ec15065479fca7f5a748404d765409ab7de39784700d840feb1bfab4502c427f28c512663406ea4b06ced06ac73ecb95d05515cf26821f3b4b0062fa93ac3d294b1d11bdd7a09bb934c170c60e2e03bbc274b3618708352a01c724dc9a9fbc6872f85fe3ad7905b87802d0dc73e20591c362ffbc5efdd3de875066e80d102811caac4df49f787970ed19709c74b9e08ebda32b20b57838a45e66b9c98a7a5aa9a6ce5525021baa00e04a703f11805594722ac1623481ab6bd47e2cec64f3378c62b20d89ce8142676a2e50ad6b86e17cf2a1cce6fb7c9612b594c8cf0abe6c9de81745fce58fda595ba4c037e9f5f2651b64f1bb2a5321d17436ec67a56f5e5e53ceed75d9e08764649110c0da5c76550f2158cb7ebc1a43f68af46c1fee9a67878cd6eab1e9620e641920bb51bff5c03414b33a3e43089f4e3c8f459794f454cb010881b78b7c6cd6d510b49a90420f81b90f2d3f137ddab1c6c9dff80d2836795954bad8631c6f773cd048b1d9499425a78107150db5348bd780b9faa6ef8e028359f52103d2e70b1ba02022e3f58d58ae252dc3e7978f93ad7e851b04165af9de1935774049a6b1fb60f3141b0c6d8c04268fb2c19a1cf361d43b3e3d3239aec3ab5508e83472b2de2031b6323c21d58e8318fc1d0a5f83a26a22267b98c3346dafacf3e09a380a68c06d5b433797837827e1b55cff81d7e1c87c266e05e6f0487dbdb6837a89bb6cb06807b7821d80f39e367f21e2db437756c549a501ef179787b94cadf07d534d803ae891c325420d23f3aaa5d42d0c02bfee37a74d3f99a3a6f6b30d8250a46000d082601ea072941300083bbe3db5fefd7cb92bbb09056df8857d5d2856722f80be9b88d086e8e9c7ef50a9af919ab516c572a9305ca4eb3001311e2e5747559098eefe9df874d5140cddf2a02e9b18cbd2de94f84637d866342a1459e2daaf40ad8e66c43d2dcc782f6521b1940d0bd56e347b2f24b086b7800a73d59ffb26d00c17a725aed1b14701c371776e7b0fea883ddb8d4a1dcae6247fa738e2535ab01e378bcb749233d1c8109cd1848f53861ce67d89cdec9cc034700b39509bbb7035d1176d942a07bc8f3ccc732465df46a4c28efa145b17e1f5b98d80363fb33077bc4b8144a38384559e633e2d4d8da9032c2c5632b972583ec3a56e436dd0c031a39f34f27e96baf0364761a91db4a7cc07e7b26425304ec55f68a3231014404af8be2b3c5f5011901c7239b8eaa863326a11b025015f3f9f10f3ba827afa9054b7e5ca93bc4e3822ca250b8fcd29b6cf020f04ebebc31f986fc78e90485e10cc20dbc0f93ab7be94b9ebeeb44ede7e4b515fec59d7615e2e325a53da669dc0c63fe3960f00c2d192b2f0b8e0ee240e7eb27b30d88f7c7b56ed575eea7a48a09fb8549844629cfe27efefd5d95ef0b659a4b12b083328441b919f8f26ee88f0767dfa7054b81f2bc02c0a86a7a53aafc35a0340ff25e52093abf12a78574bd0e6c1040b05a4d0281e47d7e08be20f5034ce1dcfbfee4dece5070e3252960d00a96db047007c6c519b8ac22772a70eb5203187ab35b2e3c17a17546fe90b1610711cc31251d1f8142a7733243f0e4105e43158a8579e2502268a9533b1cb03c002542a4682f33deee64a0667147d0022fe957f6e0668f245008f7e84d91f8200ca3ff884851b22c34f635b78c585bf7b77535865d9a9e37fd055c5f4b70c76e0a032357d0f344efe50d80bfbc558be0635c01ff6db7af5de75430e73c7d6a560d3195930c3dcefa06a471c713adf85f1961c1863296cd92d0c4711e37b267200b92469eb34753be260e25ac0ead010c9ba2a3f93e1a276f100b5f64c927d7b30152e902344cf4a3255a61fb7851ae6f1ac98e605a5bfea4fcca11b3025b3240098191b5e0f608c2f3ee6447b604c5f99889f8221c2a4841bb46db43faa566360d676bf899f2c42c63ecde2231c0fbb6e1e034ad7d5fb7e9438a614e56ca2f2a0ffc6f03356f5f10568df4cc6c0a4119e6dd3a4fc1742f7b4aa127e01811307106cd1cfca2f6462945dae6e2336ed118402b50cc2fa0a0e1a5dad3384d62e5410bbe8ab2b7cea37e7fd862594fa4ff59482f0aefae72b616ed1503d0f07752470a17654b4462afe8bb01964a0f01d68c79626d2e794260926b8df939491d813d0eb4cc839f46c76926b5fe01cc89db7ceec994c1f652dd448fe6d8b7eadef5270d70dda63f5e9d8667ac6e55af11cd03c363ee888f6d0ec0083716dd511346d30aeb3b0ced036a4839ba826ededebd2ca6b1672fd4eba7387a973fa6ed229d760447bca43bfbedae484c22d244822f98ef9cd3c76a5f8fcfc05a7b6b93355a92000c98d70c2ec662f025644ab0694af26880d75dfed96e1f7bee8952cef4ec0700cd5aa51a929bf957530f0e884cf3d5b7b87225ae00dc95e02255cf7ad1b87f01d6bf94c16b9770586fdffe5e053304dbdcf180cafe8c40a9782dbd4d494ccd0778298c7b057af9d6b9b557345665aa942685a8fe187b15b4ebb5df5a04c00e000374bda7d997e5367484c26290c4c207e43eed04665d72725fee7939b5693a0b793c85cb75780e552914c862690c1a98d95b5019a1452f385217ac465daf600b0b294ad137aacdd0b228c8c1e13aaa2179da318f3026ea1261e5cbf89bd0940d0f2e29b903837f851d81c86258a390a24121822db497535f7c573f8b16cba90a1b682d43aa395902c22c13478452c8f7e9cc1c679e28af7b9db122f42a631d02b1a14b38094e9fc807baf629603a42080bd1c6ad22e59de6168cf414acc37c04cfd886bb1420fe05945b4348680b5b62b2e5c76bceaaa7f8fc232f27ec59fd0e56e780fc4040fd9404fecd4f99656138a1dffe0e2e9904b90dfe23dc3691a70c917c46071da02d3a390f0c37fb5a85e55c5e3e9ea4afbbc0773c5c5f54715b0149d8bf59feaece9a6e067e8d496f55423c5183e0f33b98d115e9823dd65fce037c8c02bfb6e301df0e2a77fcd67531cc87b2c207c695c1e1a016a93c93abbf0d4f8de6908cc511a761a6dae25b856d33e378e42004d5b8a4297ef3ee87507b0c6c6a3a15397ae01aa07f301dee13283abcb71f751d8f10cb8cbf479856e395061d6545533cee3c733a2ae99b8bb2293910c8452fd224604a07c695b27d98ab01d54d15735aeb41bfb635075dd763c27b842fef5939d0208dcc0061f72f30530eb18ff3b02668b2b006ba0f638b2693c2217266fc715829b54eab923f0194d90b7f56dec1a64620b7420b8599e6497997aeacf9b5232586ca83934e13c6275902960b62039f93c8ca6b2b40c2eed1f32610bd7a72e7f36c2ba58a75287a8e840d2caf6374ff8f6ff0778b937ec2190dc88a5d086289920930e92e226b42dcc4046744ea55a01c349009c907de7f849ea9610d467c47e989cc477127df916c350dfdbcb37c6eec0051b38e363fdeec48806dae1b858fccc372531a8a406f091202d2cd247bf246edb3aa6d43a3ed58a41b822777c0cde9464230dd7c7754b15a0feb9272d7ad16e9f27456f6327e6891e004a1d03d17322f6051ba8f541f6aff0c59c9d047963ebf84a7a6d69ec8a1d686518cd50d0de117fe71cb5f16e9ad0504eb4b847ec02e17ac2b1fce45bf3367efdca302a7990bda824672408c1b99390ec8e834b70862d5337fe57af43b8e65dd1939f05d78ef469f0e5bdb8279e8de0bc10b09d17f5efab18e273e5f6253594d09c14d7710245223c6c435019c4dac03dfdf1b29eed77d408169da4af23fae0778a02678d9b2b9a25cb87a80507b24088c56977ad9ddc6fcfea051924c2860914189b4214989099e97a9eda1ad0ebd0483972d1daf88c038626aac2e16f17d05ca69028ff9af5bb136c288bdf8b06d07ccd342032845c4ffe738126a1029d0b96508d17f9dcb3897d6179c3f7c45de0d0be85f985feb78fa0050d5726aae35d4839f49537e39db67b16640b543ee320d8f8f798f5ded09ce1330983efac81436d68f6b20a3240b42bb15047c244a380fb4a2bf702dc28f89f5ca042e1568ed0bbf7dce5b1e20f9b920356c0fe04c3f049777406ee6653c18fb65f4d399abac285459e0ba94410c471383dcfa75468a0a328fba856a39fa07540886879cc0efb62434a5b7ee4f428b4081d92554ddcf03db8c26b9bd1d568268ca14787fe68aed7a9dd997c523c0399e99ff557cf3db0fde3665bb30c3480fe45735a4f36dcfac8114fab3393eecc10e6893e517dc4b0e2f039df9a7b5eb701a2cf4ffe49a5f2440fadf5513052c174f6ceb16bfe7fc0915a0e53ece32654c54943ee340e8d008d4d03b86709b9be778663ac64379b30df772805d65f2d290eddc4ea15d1c3951fd1799b6dea36d3c5f55a012f92f240dc545ad0317b07ffcbd11762190f656b754b1cc74495e913cb2c633821f673903647e5d052ac103b237d979766de22c0cb18808f094d0056c05923845f8474101b2d667011a3c113470f486492a1853dba36a60b8dacc9f7e9d6f707d500a7f0b6f84050d83d36eaa00e6023e2a1c28660fc143bfc86ca36fa929e6871b95b90dc0b9b87f4ba80a3b80e4a82a92c61249e73fb7997e156f4600dd008eb1fc1f08afd11700d81c4b1e52f0a84fcfd0cf4f2964ced8db1f09eb0acbc43ee69a26017bca139f10d8088525d6c073dbb5ff74b569c089ea032b6dcadd169c89d0c108848bdb2bdab72c436ad4bba3b64cd5f2254035edb48fda0d1b9e981f21aca4029b244427237bdfa072c162f9658fb54f967bb7a2e99a6adf6638991ceb525c079980afaca07079aac22c9dd8ef10372ca5f413a2b6100fa9183a9afbaa2d8b0c6fcec61282d98a334c86e96cd00d0c73fb19b4168e37819fee67edbbe9936404297c9a4c84f95e2f87233bdf2033f86a508867358b80e37c3b655454216ced0de9cb9221e6c36b7212cad27e4d96eea76b09c8bf65f7632d00e2c948efe7f80dbca2b01d73410941012bf43ba9c32327979a56aee451c1573c421431ef22e806a3861bbea4b075a6c06dbfca5523432f6a3e4d9c27a6005f74fe28cc8dd23509fdab94ce9fde6c914186e0b811d4bb132951416811b9dcaa2b08587a4e071101ab44a2d0b384605790355060e19aa5b681e4ce0dec743794a7144b2fe6b98a001054d21329f8cb87ef7ab4e1bd28e7b270970512b7a651e6208a7d537aa5ac09b59c58eabd8ecb133309d0ffd5b0f9a3ae6ff0aac60716e5724b14a165727100128606c2515069ff8f070b9ff12aab3d5278ffb0f8a522d9a0ee1387fa31900ff8ca728e89204c5a32c8bbe4be55561f9b1e16a2fc23a8cb096cb0bb656e1900475098168153658fe4db62dcf5c9ed4434a853f6841058b7c9b1edf3df219306cc3648248b2ca6c0840b401c734881d8f45f2097b3de9eaf69ceab7aaa75ca0da19dced128302f3dad10f235822037f04363bb39b78033babeba172b2e8f740679efccf8706661c97452ee49596f114060755746405005527dbf3a56213f3b066183d7bdd4e4b4b199b6443343995d52259e459cd21661342606e0c4252d6406b526ad202b57bb15ea9b75e490726ffe01460775b48ed5c1d2dcf2c3eba985071cb1faa1e28a618ac1fb4835f8ec7ff6dbdc05d285abc6d25fb6f97bdab52e0387a8991630b62d6b395da7b0b1fd7d2041630165e8c4bffe26346dc41b6e280c826eb586327bee362889a7842c196640b754cac4d2db6704d58dd268e328ea0121c41ff123053ebc279518e9f18be176165fe114022b6f7bae326f83bff22f0f3a70720297dc78dd7862edc3f744256a08527140f39c25797656319f3e547202d9f833f6ce709be923f5946dd737c182767f588a91ab6a949124eacc7fc03504e3fa10d7d09987d2c2b2369d9c6b58f95a725aa17f8cb2f51e2711be391156035179011faec7cfff5d024e7653c32f359294e4fe00e6597ab4156bc70d1d8b0550424946218324f1293f3a3136b187530c2d10c5a20b0237bdffce9a79efdf0cbed0db794f08e9b5a7f62f0e057e41210704c3b6c1b2e3a695db093e1219950371414a321a81d8d01bae58fca73fb9a068cf21166a4a650c704137d11b30ac0c211826dc659de3c37349c04b87ef3d58424b4f692d7b96d3af99ee0d06b5d60ca3ceb1869109be885825e55d6571b714da8f750ea4c77d16d45fe26dabf29705d25b8fada7b9c4bdb587fba726916405e8d7f484a83a2e82ba5b183c84f57f0821f9a1528d34646c41b64bc34af7faa1da1d1aa6b8552e53360b4135570c1108d06975358ffe797aafdfa218ea1c3e146457287d1504ca3fce3aeeee9f9e190391530368ee632a0b964cf978274e28787d6d2b69c2c5ca97bb98d17a1e184a0cbcd66a7e015213318afb0a5f0adf55a54c81aa2cabc6d3754418ecbe3c1edd018133d21bae9d7dd6925bff7d8e880dea8f609b3c809f1df8482d01312c64730037cf6fde50ca5f7dd8571026f8debd551ac5c70b38a807faf460dc59525fb40f9b8cc04dbd70a657e9754333fdf3a5cb47663cb190e919193b551ca86091d40b6f485cd312d20bd2b894512d13b95cdb3f7bf45eff08096bdcb8ff64420a9f01a31b7790f3f6736d99cf07bb06239de5bd86f6f821a3a79b431f7c475204d90ea8161ad99f27c67ef20d6cfeceace586c44f365d98fc539de4c53bd020ec1c0a1241d80ea6c7e266dd31ab8060028113ee727e10d68925f03182d9962556ee012fb2e771bc79a84eefa0b1752272bbce96aff777f15c188a17426d96591af50cea8a9cb2e2c4626bcde9f14616b640f9906ff3e69887f9a1b71b9a861aac7e0f63bb64c168d1a248a71105ce5a5ac0e5a7639f9bc5b81b003600e3ed7ed863048b8c7b94694fbfbdcc8db9b80e815ce7215f9ac36417a764d1b55345c75f9602f0d62518e8c9bec38eda3b06220b08032b62f5a75274281f5d1b49927be1f10fd8aaf6e39c4029bb069c32cf3a6abefafbe919ec082b2f443437bf628814ad081f79e16898ea787c87e8116e2d0b65aced37a75d045f2ea7643c314d10e9c60fc9f4080eaccd5dfa0025528bf5d865db9e16e3d7a2284b937d72815da7eec108eb864cd01a3e333710de6d2922136f48ed876c1a21033df64ce50ffc1601d10bb399ca22cb3b2a345aaad6f960a0e5ac4fb82aa66d44ef6956b162ed0110fa0aaabc82eef48f7c9b8d1a929cefb8a220d3814bcbdaef52b1393714738eea730b1e0a35597fa47021a415877c8c90b7a0135ac18bc52e17fd4d72e1ee33e6d500fa565e4e3268048c58544d950d7c83860986b896c3e5b3203ad33661c3d9a506c3982589eebc9423183ea570af3dbfec1e216acf41ea5dae4b2fc9bcbbba060a1a23f0fcc3355088a82e88477ad41de3a64fafc27756021b1d40c6e0fec4740c25d6f45403eb994359cd371abce2f20f8ac8c5eb040f47f8d795d59fe9b45004bd4cc04e00ad8c47a93e5473a41c1b4a38a0383558a1525acbe63cef822220092a51622797ddaac0a61f518597d6fcfff71db744346793bdef49b2cce08b93088c6dc27d816dbd44bcd36525ef6e625ad1ea8543f31ca0d91b0f7cf892d0530f247a4fd50aa7dc94326c62a965e40c186d31f17c769e072800dd6f1898f44904b40ddba5501d4606524a04252000d2fa8dcb6985f7d42e6fe7359f4cc7307b0e9b6b146943b9777bd5378c1a266a6daab780294b670368b2eeb431d2bd1f8d0e3a0ce56744f176a6e691db889342a56ed59dbdf08b7a5ff2db9e10718ce69401674d2b45a7e638305cabb8aa4600868bccdb472dbeef378e8b9d6474f32a7b003ea686b61f51e2102e8bb94eed7f1220421c85689c2a58fc114cc509427082098750c5c7bef2829995c13c6fb1eb992b45136dcee518f3185fd8ee461614be0529d4fbed1731f02e767d821fed16f4020eaf1eb42b6602eeb944d197b4b6840c");
    }
    else if (GetParam() == 1) // testnet
    {
        // tx_hash 0124371c3470a1e79053aebd9b9fe859005d7a7cec786f08655ed2cebbc673e3
        txs_hex.emplace_back("020002020007da9012f79602f0e30399a20314d7010cd033d24cf85e0913d79de224bc963260ce2a9525abd20c820321d521f87c54f502000781c609bfd508d71a9efc07d07741df0429e0aa26efaeca36c9f157bf8e80619c32e25a78a4e724815d892c857a2f8766020002f1a558f69093ce2bcf02719bb5a372a9e2b92651b0c861674f4f0f1ca3ac2ba80002cc84eabfa99691044bbb314c08f7f3eed2b7d1c64fc4733c7f7b25f6b0f212812c02090180006f9a80dc79e701eb19d03b9110895caa08788cade55a1ab50f11be91a5ebdbf02b36b27211485404d081f896033702be8d4330b04466a69ca94779dc0c9066e8be85b3513ab6648128c64a80080a85345cd384aaba7c8ba92b8d23a21a4ed27e38134cb3cd3c4fa5701728f80260f567f28a61f9074f1a14ed83532dcd67fde76484d2969a048160799aad8503b37aa103cb3f122b9a03e99ebf890f23e4e0284db56b3965f1b4183edc284f0ceba21af74ed66fa14ddd72e3e07b78abbad53590565747764e64c04e2ba723b0c39894754f95b5ba6c01b7af7f152298f47549cd69e0cde657d29399f48087b1434d93184e0b8d2549772302cce035c9f625ff7de363715a6bcca13ceb829f1285d807785c9729a1e6ee89fc89737bb2bd7a8d66bebb09a10a9090a55ea375608da1c73c77c64ae6dc0a7688a25d6dd8dc998edf60fb51a421c78bab7b698cabc086521d7e78ee5a41ac93955ef4436216daa5af1df1ed7a48ac6f52d723d3c81b13bdce40bf973dfdaae2e29312c8c990ce058659f18d71b75437ccff40480595e13219920f6c0afe7f8290dd1d9bd875f5043155042a8f96a77878422fe10906a039cc5a206002d980aaaf82ed6e650cc3fe27529ea412fdea7bb87a1422c047741b98719756b8b91700a40095cca780cb8b0a6ad77236c8d2ae1e900f1037a6c9098193507cc552e77947c6d8b00407cd4a91d551e2751a2c248c51c77bee75f7195314c1b06813803f3625746b1b971f0a03eacc3b50600c7759ee39c5228e5c76da2200179f6db8ecec631937412b992935a33548af4947088c31607d22836ff5723a0a6d7a40fd7adf11d354e85cd3d22d1b77e3427c3afe648a7c02286706e24453d75278e499bbb4dff1aa6b10e0e1b6b45804ad909707eb663a4ab9300a72705c494bd0cc8bf1b3e0adb4ba9fa432b05abe766115ee8e186a445d3c7048514b012cc74b1eb261f3344a7691437ea2912be0bdff79aae16cdf3b9192850a9f74bf0134e9568acb39aface8e4b571342cc3f86f875735f54d0ce7d263620badb99209f03486de1a6b2c6636870b187c75e64ba51455e9fe45f2ca9d3e44efad80126f9ca4bc02c0a3bc8de68aeb41616089c8ad2e8b88b650a47438c46f055396a378d407b7a4fa0b667d66454f2c7f3e313f72233878e591642f22817b0a36ff4c1d04f03629852509a7a0a9c3f68a423a4ee407b12444b60d2b7db4f902b566258973dbff3f718ca0ac12e7a32d2c88e638c14227009bac9c1cca43c304e48dc6baf650c46a7abf45db4f960a787ce3bea193a39833d0dcba7feb7c3bc322666e4b4c451803252cc4787e53b1754e795e75afa4d74143e59c3820f4c66d31ed635833fc868b200ae59e9e718393cbb8e7a9c5bf144cae1a6b54f812cf99fa8963d977a9633128ba5718f8fca2aa8064d3357c23a70d32ce59e1f3a0c251511ed9780dc1e99ce409773030121edfe31d9819e3346bf02eaa536bc900ca00f8d4c40283564ec5acc82d1a8abc69411ea39d1a4d89ddee2ebc99979147380206565d10f81cc26110a77072d8097e6f6472195465677345e4f9bc9174b7f93088fcde21fe43f3972284e9ef17b3f821397dded316c8716a114e9f90b739b95605969c7e69d8027bae858c1411367b200b3a0be7dcfcbea6e3937388b6e5861d78749ab502610f6a05e90fa525a01272d5345a6a8b659b7b8d95995d4ba4160a5aec0060c92cacf82e796cd798c57fff75094435c90b7ec6fdaaaca33ed2f838f42da96966e0e33477b91c7c34d388c2eda1b18d1c99457281fff70406a05dc18306aecaf77b78cf7fc7a39e7bf7eef38547fb7f0055a768f870e60e0ece6483b7ee85808f132d0c5d013ab929eff21065d8679a751d882a332e27a6752a26c5279c846591f8022f39017e9fff4f29d7dbbffdf4abb1ae93e957903a8d08272113da43fca3d707b56b546e71a1cec088adbcbe136d5402245c2545ef5ed0c9abd8efc46fd323f3da50c4530b59408d7ff08891b8fd3f2ce2cd6938f06c884cf61d974acfec94b6d9e80fcd493d5251750b9101aadc749157a19d2b8fb21e33dd0c2161839898d71bcd9f7906a680c55ae03c9c8ff771f7943d3c8c22f9b14480410353b2ffe7b3ee174ee476fd757360475af4fd419b79b91f00f15c5e59a415e80e32508152be103e2eb487fd0e3bdb9474f2efdf9aeafc2075fe796a510009e2033633390514f141cb82861aa8122280da1ccb0872beb671fa63f860f18194fe08e34d40154fd13e25f09d4fcdf0dde7452803de730f315a87f8fd6369dc20560542a9a8a5816923d730880bc242e0f55a82e44050035b5ff4c5f8fd5b027ad00d249fa3006150d035ca82e59ae7ea97f77afc6dc3ec197444b70a8603875ea9056b7d94f7861ab3ca1106ce3d39af4e20b62d03d2ef0e14d24a020320412ed40748f348a97739cba7c29a24bc2e93f293549f2ff6a7ad2d9ac19360814f659507e64d7e8affcf8e085c543292be5e226138a9938b7824bd7e6e75499c8634580130e846097b212d860354b65aad7a5f1cce9a241eb541739e8e43336a0274f509e1155657e032ab0a1f4fd4d2a068838ab82588ce90ec12b9943d35661e3cac04a42c1c73e395004a38b532fe77a85809246a0ee90088f1f09c6c0904621843026b2e706d2828b474d0d1162251ed644ded5cbf57c52271cdb46c85653620a8019bff6bd799b38deca17ccc17a6cdbe20e3c5559341aa56ffeddce762095c4f07f211ee4af5fe893b77e5a3bbb43720065a3991dfa463a937efd77d38066ca70afa4ccfb358f347a4b6c1ca30d04072a9ff0c1747c88492a533a99e0cd48a7600ed50caa0dfa184e83fb086508353a187dcb09185982ed176b50040df6673c5035878a96a6701a799f022fef2a540030809c5d636755b5f6df32851fb6e12380156b3bec68573241062dff90d73fdc799fe64838343834f4f7c2cda3d3b67770ec6fade3173f4afdb7f09503fa5a7ac5d6101fbadc98d42e9d24600fd8e238d0eabb212b10a8d567c982fd00d2a87264b363794a22de78d6999158422419b860976bd4e62e2d58a0a155d0da1bf5207f0ef5e4734b8d311b589a0d4005098300097a466d853207b218d05c68d795ecd90127363ef103c2601df22a575ec713e08e7f25a913da8f229865900a58ce872c856b5bd2c8d224b0e9dbad742b93dd20a321280b6fb0924da6cfea1d822d6eed6a34a5f471699acc1215d53b7b41a05064c719793f5c41c9db80b766f68ed8ab6398bf06c741f63ce77ea9d4bd3f5760004e09838a2d6d17184c430bdcbd574b24adf15a59ca836697d6a82cc40181b00e383ec21b8e1c2d5905798e43e9b1fdbf361f0cf32256d9f2e90101e84d7e004003c60a159c2c7d4e64cfea5312ba5766e9b622de053ea3433998395eab8750b95d26d02556389be7b830827308c2fe0a3cfd6308fc1391770241c863a3ecd0f8cb70912637d451fa0cac13aa0d4e5fd5cf4b64f6e43d05ad9b2ad6c833d220acbd162e9e07217c62ceadf6aa895141817d440df5a58cd60718742b2ae907c059150ac40c56a1a620190dc5096cb57d731d0451f8c4eea3dd30e13000dbf867af44b20350b4ac8ee6cad64bb954b29deb18e6e616debb34e2a2a92814378270a");

        // tx_hash a4080f0a37837f24ff4512cc0e21c5203e7445b9689303e7add5702c11725e9d
        // this tx should have 0.1 xmr for stagnet wallet 9tzmPMTViHYM3z6NAgQni1Qm1Emzxy5hQFibPgWD3LVTAz91yok5Eni1pH6zKhBHzpTU15GZooPHSGHXFvFuXEdmEG2sWAZ
        // viewkey: ae6184287ca791609844f140b8502ccfa2223c04c8699cf31fcd0af1f1d0be08
        // payment_id8: 4c3bc77b30f486a5
        txs_hex.emplace_back("020002020007da9012f79602f0e30399a20314d7010cd033d24cf85e0913d79de224bc963260ce2a9525abd20c820321d521f87c54f502000781c609bfd508d71a9efc07d07741df0429e0aa26efaeca36c9f157bf8e80619c32e25a78a4e724815d892c857a2f8766020002f1a558f69093ce2bcf02719bb5a372a9e2b92651b0c861674f4f0f1ca3ac2ba80002cc84eabfa99691044bbb314c08f7f3eed2b7d1c64fc4733c7f7b25f6b0f212812c02090180006f9a80dc79e701eb19d03b9110895caa08788cade55a1ab50f11be91a5ebdbf02b36b27211485404d081f896033702be8d4330b04466a69ca94779dc0c9066e8be85b3513ab6648128c64a80080a85345cd384aaba7c8ba92b8d23a21a4ed27e38134cb3cd3c4fa5701728f80260f567f28a61f9074f1a14ed83532dcd67fde76484d2969a048160799aad8503b37aa103cb3f122b9a03e99ebf890f23e4e0284db56b3965f1b4183edc284f0ceba21af74ed66fa14ddd72e3e07b78abbad53590565747764e64c04e2ba723b0c39894754f95b5ba6c01b7af7f152298f47549cd69e0cde657d29399f48087b1434d93184e0b8d2549772302cce035c9f625ff7de363715a6bcca13ceb829f1285d807785c9729a1e6ee89fc89737bb2bd7a8d66bebb09a10a9090a55ea375608da1c73c77c64ae6dc0a7688a25d6dd8dc998edf60fb51a421c78bab7b698cabc086521d7e78ee5a41ac93955ef4436216daa5af1df1ed7a48ac6f52d723d3c81b13bdce40bf973dfdaae2e29312c8c990ce058659f18d71b75437ccff40480595e13219920f6c0afe7f8290dd1d9bd875f5043155042a8f96a77878422fe10906a039cc5a206002d980aaaf82ed6e650cc3fe27529ea412fdea7bb87a1422c047741b98719756b8b91700a40095cca780cb8b0a6ad77236c8d2ae1e900f1037a6c9098193507cc552e77947c6d8b00407cd4a91d551e2751a2c248c51c77bee75f7195314c1b06813803f3625746b1b971f0a03eacc3b50600c7759ee39c5228e5c76da2200179f6db8ecec631937412b992935a33548af4947088c31607d22836ff5723a0a6d7a40fd7adf11d354e85cd3d22d1b77e3427c3afe648a7c02286706e24453d75278e499bbb4dff1aa6b10e0e1b6b45804ad909707eb663a4ab9300a72705c494bd0cc8bf1b3e0adb4ba9fa432b05abe766115ee8e186a445d3c7048514b012cc74b1eb261f3344a7691437ea2912be0bdff79aae16cdf3b9192850a9f74bf0134e9568acb39aface8e4b571342cc3f86f875735f54d0ce7d263620badb99209f03486de1a6b2c6636870b187c75e64ba51455e9fe45f2ca9d3e44efad80126f9ca4bc02c0a3bc8de68aeb41616089c8ad2e8b88b650a47438c46f055396a378d407b7a4fa0b667d66454f2c7f3e313f72233878e591642f22817b0a36ff4c1d04f03629852509a7a0a9c3f68a423a4ee407b12444b60d2b7db4f902b566258973dbff3f718ca0ac12e7a32d2c88e638c14227009bac9c1cca43c304e48dc6baf650c46a7abf45db4f960a787ce3bea193a39833d0dcba7feb7c3bc322666e4b4c451803252cc4787e53b1754e795e75afa4d74143e59c3820f4c66d31ed635833fc868b200ae59e9e718393cbb8e7a9c5bf144cae1a6b54f812cf99fa8963d977a9633128ba5718f8fca2aa8064d3357c23a70d32ce59e1f3a0c251511ed9780dc1e99ce409773030121edfe31d9819e3346bf02eaa536bc900ca00f8d4c40283564ec5acc82d1a8abc69411ea39d1a4d89ddee2ebc99979147380206565d10f81cc26110a77072d8097e6f6472195465677345e4f9bc9174b7f93088fcde21fe43f3972284e9ef17b3f821397dded316c8716a114e9f90b739b95605969c7e69d8027bae858c1411367b200b3a0be7dcfcbea6e3937388b6e5861d78749ab502610f6a05e90fa525a01272d5345a6a8b659b7b8d95995d4ba4160a5aec0060c92cacf82e796cd798c57fff75094435c90b7ec6fdaaaca33ed2f838f42da96966e0e33477b91c7c34d388c2eda1b18d1c99457281fff70406a05dc18306aecaf77b78cf7fc7a39e7bf7eef38547fb7f0055a768f870e60e0ece6483b7ee85808f132d0c5d013ab929eff21065d8679a751d882a332e27a6752a26c5279c846591f8022f39017e9fff4f29d7dbbffdf4abb1ae93e957903a8d08272113da43fca3d707b56b546e71a1cec088adbcbe136d5402245c2545ef5ed0c9abd8efc46fd323f3da50c4530b59408d7ff08891b8fd3f2ce2cd6938f06c884cf61d974acfec94b6d9e80fcd493d5251750b9101aadc749157a19d2b8fb21e33dd0c2161839898d71bcd9f7906a680c55ae03c9c8ff771f7943d3c8c22f9b14480410353b2ffe7b3ee174ee476fd757360475af4fd419b79b91f00f15c5e59a415e80e32508152be103e2eb487fd0e3bdb9474f2efdf9aeafc2075fe796a510009e2033633390514f141cb82861aa8122280da1ccb0872beb671fa63f860f18194fe08e34d40154fd13e25f09d4fcdf0dde7452803de730f315a87f8fd6369dc20560542a9a8a5816923d730880bc242e0f55a82e44050035b5ff4c5f8fd5b027ad00d249fa3006150d035ca82e59ae7ea97f77afc6dc3ec197444b70a8603875ea9056b7d94f7861ab3ca1106ce3d39af4e20b62d03d2ef0e14d24a020320412ed40748f348a97739cba7c29a24bc2e93f293549f2ff6a7ad2d9ac19360814f659507e64d7e8affcf8e085c543292be5e226138a9938b7824bd7e6e75499c8634580130e846097b212d860354b65aad7a5f1cce9a241eb541739e8e43336a0274f509e1155657e032ab0a1f4fd4d2a068838ab82588ce90ec12b9943d35661e3cac04a42c1c73e395004a38b532fe77a85809246a0ee90088f1f09c6c0904621843026b2e706d2828b474d0d1162251ed644ded5cbf57c52271cdb46c85653620a8019bff6bd799b38deca17ccc17a6cdbe20e3c5559341aa56ffeddce762095c4f07f211ee4af5fe893b77e5a3bbb43720065a3991dfa463a937efd77d38066ca70afa4ccfb358f347a4b6c1ca30d04072a9ff0c1747c88492a533a99e0cd48a7600ed50caa0dfa184e83fb086508353a187dcb09185982ed176b50040df6673c5035878a96a6701a799f022fef2a540030809c5d636755b5f6df32851fb6e12380156b3bec68573241062dff90d73fdc799fe64838343834f4f7c2cda3d3b67770ec6fade3173f4afdb7f09503fa5a7ac5d6101fbadc98d42e9d24600fd8e238d0eabb212b10a8d567c982fd00d2a87264b363794a22de78d6999158422419b860976bd4e62e2d58a0a155d0da1bf5207f0ef5e4734b8d311b589a0d4005098300097a466d853207b218d05c68d795ecd90127363ef103c2601df22a575ec713e08e7f25a913da8f229865900a58ce872c856b5bd2c8d224b0e9dbad742b93dd20a321280b6fb0924da6cfea1d822d6eed6a34a5f471699acc1215d53b7b41a05064c719793f5c41c9db80b766f68ed8ab6398bf06c741f63ce77ea9d4bd3f5760004e09838a2d6d17184c430bdcbd574b24adf15a59ca836697d6a82cc40181b00e383ec21b8e1c2d5905798e43e9b1fdbf361f0cf32256d9f2e90101e84d7e004003c60a159c2c7d4e64cfea5312ba5766e9b622de053ea3433998395eab8750b95d26d02556389be7b830827308c2fe0a3cfd6308fc1391770241c863a3ecd0f8cb70912637d451fa0cac13aa0d4e5fd5cf4b64f6e43d05ad9b2ad6c833d220acbd162e9e07217c62ceadf6aa895141817d440df5a58cd60718742b2ae907c059150ac40c56a1a620190dc5096cb57d731d0451f8c4eea3dd30e13000dbf867af44b20350b4ac8ee6cad64bb954b29deb18e6e616debb34e2a2a92814378270a");

        expected_tx_hash_str = "a4080f0a37837f24ff4512cc0e21c5203e7445b9689303e7add5702c11725e9d";
        expected_payment_id_str = "4c3bc77b30f486a5";
        desired_amount = 123000000000; // 0.123 xmr

        // tx_hash d2287b54215d9d614a9a5366f9d786cbf03a1b45143b6246967a8661404bf62f
        txs_hex.emplace_back("02000202000788de17d55fe0779f28a894018a1ea41ff04aff9a245c5442fd58b00ec7ed1acefdd70babd27bc9996683ee71b02791e002000785ac039cee08ace408ffae028bc902f9b1017de457e596ad30bbef6b728135ad7c5a99587577f67bd247172d90f5de9c5c96ad0200024fa7d9ec3be18496b1687cb81351214095a12525cbe07665a271316732e269bc00023f817e4e696261ece3bfc9bd336e6d8047cf721b3d11b3c6c8c86092e654b7252101cca97f40f79fd56224be79145aacc5caf3da52562fdcd92253504b71132a215f0480ecf99603f8e6406cafe0b2bfc38184fd248f3d453943c8d7a82f6b04884e152b48c9be04a365c2158f93a92b40db8d48d71e1afa823e8fa62078afa3246863bb4358c407267c9773b19c8b106facbaea01db47ec18dfe88e64be035c420f4cf0f4aad102efcea4a08b6e28a66683a6114f578091c8d572a77ae27611d40300570033760d76fcdee2ca05378c7c860960ad9d82a6b98e49326c163eeccb51336e10feaef783bab3c64a9ec6612d20e91fb969780a8cecf6c6d4ac1ad2feff092909a60e5824fbf296b47bacfa689e44e731b258d869c570261de2f7018ebad487e9c77ae7eb34ffd04f560d8c25cc3a9a92feb6a3793f96a8116ed86179252553a2b083693e8d6753a66e444a17ec77d2992185943d0ffe78f4dba665c0cc0626da0ed14cb3d97dccb3e91273388fc3a56d21220704a983d03109650b2288338d48e20b2a38feb0667cb1a3e53453f622c83809efdd83d4335a4e9f3db35eaa32937ac30794a36c4dba20fe3e71a1c3bc9e7236c6c669dfc3472564659b151aa79fdde30c067c0f803098d24a303848fe2be0207cdd8859f2607c439c4038c106ea9dcc12ac685cdcfc385ea55beb0c92f144f359518ab867c894d2a9ef94154b4b10601aab08f5d872cee155777994af79b6cbef0c1393e290a64e8916313b96ce8b87922137189362dcce8283bea8ef56ed4ac3b2b05d66a4f3bc1a352194467b69905fc48b6006ee83246695b4b797965d2d050f7640b9f15e7ac6ac7967d6f393c0530e23f68f9f2b2375ba83e3449997b67f73ef987f6fa73bbf0e1aacabad150776f106cfdcb1ff80e9b0d38fdb5d1d39af212af85453b1c39ad2c6eeb225b60231aea27ff2209c453e620967250b3ffe00e91d24a4175972d7b4214deaf5ab78bf26346e9466b35ba3b09a076d0ab705a117e3c914082c36fc11afb9ae21cc35000ae83716aeeae17ef6e297275f2b66d08cac35c5ba7e164238f9b0c3b0f26f5e260257995637de484e87944b809965229150a92696380ddd668f0da7e23deecb565cc8fc83b4ffc0b37508ff8804b96e0998ed5561166eee479a1e35f2cccef5e714e4e674d3a6bfda3f61bc5cc44f948618096c69b7e18daba5e90e4f10815c5b0a3dd8f83f19f1ce17044aa8dd916380f2a0ea255fc9a9d333b2f4b345cc342102538f05a53efb7a792ca73a3d3169b07be7ad293c6eb25bc8311dfffaeef32f02f8c42635c8cf62697c49461fde9c96ac10d00686e4fea0504b26174b7c8318274d5f22781b875a6724254e92feb476072b886d45b24ac42e05454fba09158562faf7521ca452fcfd5748275114fca329af26ce1e2502d4bfb09e094126feeb5faacc2ee40eca14debf5849a4184da52c1640a300345976a741b4e0f0e3c0fcc62d024cabbb93cf595db27df949325ea69a4e091a0e9b5751afa1ec841fed130a0b102ff2ebd378ff0f226523cc6aa283ddd6704d3ebccd72fa3469ac292a9e09060588a436805f6d8d4661843d1b21cf06e19692a5f9dd314e91e92be650bf9eefb3d90cdf15d846eb66f366ce72d9658270bcf748415d73c70585d2e6b5aa0cc40714cec892da2f8b6cca7bd58140fd2799498b0a8e322f6a51a21d7e94b0a37b295664195c4f404e5bab5194138cc1c5d7bd13c4b03528b3484a9ea67519d29c2b15dac7649c3021b6e0d5c3e35501c53bbbdc76944428c941471d10182a904e2e97fb9e71f9730a2b729ca0a57eea55369b6f6a2fe1be86dcf811f8b5505703065bb366ba0a63c7de8fae412b48b66bfc5315ad71673934bcab34ec2b0dae770091436f78ce5a39d9d423a9046d491364877573bef2da6491f4b129f1767dd09b5c215893424906e75802b2227dc750f44cec188fd3cd5a2f38146316b5437e6b451a96bf5ff03c822bf336814e28f25ea55c9524d9f4cc041467bf0911fc9a3269f05b93b70b455b9c3da7a4af186f2d76b66903d929df328f5e884480a975d743e57a14075cb49b665d171d1af6e531610594eeb789fa4f08c10411527a86014e8c78a2625fa3f5bcf0fb9efdc86a247d7acb5e6870d8e2df1553d2b9eea40138578424a38c1d18dd6ef2516caaf36ceb30571049f1676d1ac56a692075ed008a7112ceb04638be47865264ceab829851e85936eb309428652799ad24842c0867daaeac8f734823fbb5cb6098e19ce51bf654000c2332682d4719558bc6130ab72d838b36b8eb9a37fe97bcb6e845699d8d264ba57e954a30b8f998c5615100565b9d36c7ddd2e3d955f3dc68598465f9b532d6f9e24a1f95eeb372ab753a011d6d65a6e47cf891f614402a21da4e6843ac23caf6039ded3caf7011225ad507c0894e2e7567ecbac7ceffc2d80730e29280af11c6d02f77775d5e11d1e2160061326964dae81233054a0967048ff625548d523cda298a4ca8275920b71f850c63cb48af8f28012030f7125eab86c2dcdd7dd0e9f63c5147464aa6f37ee479076c271b76a18d1c6c15b08ddc0d45d82ece5d26464c878b4d697ceecc1a42d9072fb9c72749aacecae2f63ad4893051ee5468949fc728e22a788955c5fd8fde06385812df9e951dc1f791f9fd6d59a1d91f91b31d0a37759bb476c1c9bab63803fb13d7d6f99686dd0405f0219d120c7bcab923f2c05db8fecb6a1f75a9c0b20e0fb4d35c1b2acb9dd0920cc7b3943fa9baf3195b5abfd1986a4dc43851d5fa0360be8420c3bccb222b013f57b9d4a706ec3c6d0c3dc1f078a4983155010cb30bc9f1d8db18089c9d9982d15417670f7cb05d8e421301a67623f20132f31c550702d96b5db7b2e1e79e4bf71214d6560db6d9b43c4381cd36a8964d8085a2cf03674b20c81445c28250432f40fa0ed4af205951d4fc8260fc2473fd031f646600f13906b91bdc334635f181e0972336680a84d7c97e5ffaedb7eef30821e7e30607655be519321c7f2dee20413525e6b579ac0ca22bfdb598939d77f97d006c0db619f874e65890f240d21df2f6b6f89cd9294781fc0f1d0bf1d0c7bb222b0c0e14c8925b17a38fdb42448520f7265f642182572d7a4b200786178b886ecc320857ef51a620bc50688737ebbab8db1c7e6f58145991f90dc6b99cb850cb83b90fb8f059ef9bd394cccecd8585007d6280986fe4c71fa2fe0944159160529b2301ee9a9e1e30c4ca3b0d72aa9d5f366f86e8adfe8f672baa14ca55532756fe5e0fdabbc99bc0de5324516513543ebce6de7193c498629f9234ba469b0393a0d20430f34320b7df61e98e57183d30a3e0929efde80d89c2f62eabae0c86813ba90a3ce9ac05483ba041c74cebe80c1b622b015773b7c1d1bf0fc65f93efbedde9075345489285427e59c47445133dac4c748cf7cf26cf88b879960403924f05f2049a420a1ec6efad089742670b3ef6a6b805bdc7b04ed4fc580914a55248a68b0cf1cec2a5a55a81a1b56828158ab4671416b4b6bb042eabde318c0c5d10795f0e98ec5223f6296bc7091c679e5ab9b77b56acb05bd8cbb8ae5cce1e5425f3b90e322542325d441c7d545114ac51356863d17418fd5abbe4f11a896851b2043ef038cf6b223df2a0c0dea99800832c6b3f9b6c7f6b07d90e6874d4b70c520c2e2b");

        //tx hash b611bfdeadb1e314fb3056e4fdfd78ab642d904879897fd8a97cec1187fc36e2
        // also mine
        txs_hex.emplace_back("020002020007999e069ea106abd105878d05921890b201d607bce058c48e04877dac0e044fd3337145912e4d3aa03461d599a562e9f55a32b0020007a3f10e94f502e2488407b5f40190fb05fa8601640cfc0b2c7ab56952fa11a79e0b04f5d2f7e777ec24b83a9d274532418d28bd020002a0f9b3b009f587ade985d327a36c9f0ea961acd06ac03938b898fe20ca13e90600024f021ee3ea942b2180e6bce1d26b9f737c51d2293ae7681edb23d35f6413d9552c0209011182086f2d85a34d01dc1f51b97edb0ff69e5069bc72d5fe14356d9233ef034b2c9e63d52a2eca94e704d081f89603c9bf10fd46e222ec5f32a58f58aa25c723624d78e272992df19a7eba56ce5203f088e38bc479d3c482c2037075b2d7f42c51f8d308ebfa8b2b22dc136e5e9306dc60eaa372e78ffc608fbca486b8863dd1c69ee21ddb31d0a138edd0812cdb0a10b9d866aeae16b57e4fdafddb3ee5fac9d9dcac38399ad6fbf8492e76fd03027ac3088e043b4694eb859f65cd6c90bb3f3191bda436ed8e14af5d6c87cd44a59e235088ee138aeef1c8db94cd602633a186bf412323e01813c5d5e6b84b007ff0ef614cd34d97b0c324eae82131d665d2ec20b14c63cd9996dd6d3a60e79e4fe9b074b2d1012199dba872710f42915ccec5f9f738cad4181c09ddddb39e0815e3b139f5ce0cf2e227262b0598943e4fca0940020c79a9ea56e4f066cb512e27940e6da5a9b0a4ef28fb018842fd7c2509fe636cf75ddc47f225874c5c4a323e4057bc8a3e1567fbd15c3199b90fe2d2627451206a95a8120fda4afeaf485c0bd4d0154cab369b159faf787c9e1e28f4c057157fdcb41e4775a8dbf27c8e1d0e068e11d9495272f56a5be0967d57b30d25f32cf8cd711cd746e22ac260cb538db49d00b5f15af17de9bcca5efaa2c37fc8b7a10666fb1600253f20058c0499e923ce0293475fcd95207eb937a9606bb303873c078eea300cf0ce4cc25024b5025504888bb56fcd50e2c1ab336f50c11103b9700015fc34cfdd893c36e8c42dd4fda09ba491b56db986d2cdf9145e482926e0674b14c00e29efdf6db2f66ac4fc4f624b04a3ad7d8215c0ed707f678d378b0467a0a7891ec14e90a304f54329ec880614427662d7b410f1df2cab582d59e67c4968535c90fe3dbd3c2c60af202c6837b0bbf2045a0182f19bf8a903a972e3dc670097f8c576589c48639ed709e6d53a12b171a11f2bbef879a161e5a457fe2ada7505cf67565b812f23a9ede58df3e6f30682e68a2a761e33e9a6fb7930a9e8a23440fffd21494d4d1cec84bb73cabca29de9019df04bb3d80b9a7ec1574f1fe318215b197c3bd67c9a00587efde888f3e591abe8830cc2d81439e0e66aaade837779d5ccadee4b5352e681a66210cd01a2f054697bc09099eef433e778e4d813b7b4dcdbdcf9cb9cc88e9b74e518004fd189988804321750e5f7d34a811620750eb37355e69b39e5f16a3144062601d576d219301c1ac4d1c861dbc55e15f351d96fd1375895d9cd8592927875620da9afd7b6f5234000e1de31655ad792ee46b5d3f753a22e34d067b09e6badf04e4f2b1abd748231efd6a0518b38b6db12effaa1e8c1e84ea74ac25dc08fbcee19850717349b7203fb63cab46da07206ba1f34ed6e3be374fb9c31f43d1f8488979d59b0c8dd6e605f53372c542111a31a258ae4decfc1f3a09a58cf52a2cc3b0fc027bcd93256db09b5b4b8512335988b61c17e5e4b537bec016128ef7831680c7ddd75c276d2dc27b5786d1f1340693605326dd7f3fb5195fc41404ee092b30f06eba54c0168de43772f1a2e526f4857eed566abc7fd4e0b9ed4fbd43a2bb32f7b45373c11cc97c4a76fab6c5eb9d8bf4ebdf0c208f1bbaee9a1eb8245dae1632f1633152f8d97280255fd37b68069dac69d18768b745958517a1129aaadb1a7e220635ff7648fbb97bc2d136ded6d44f8c9a9e44c0b4435dfa731d6f83f7822e54743c9fa1b07b73f625eb37d66cc5022b756f1fe8efacefd1739b5f6222306c8aed2ece4f0680a1d0c77bcca35085eaec4a1751e89edffab8f51673ae750688b062ff2a2c727e8c52312a352fb2e019a47ddb7021a6ac5b878f80e6b3b3b257c317299114e61ff004053651ccb6c4ad1aa1f236afce29589d45661a8c550761814ffb8510345e2b574921f84bc034ff18e8cfd24d0d8c192da814900c7e8714033d0c73b02a7162c196c906c3aa1338a5b4e7661128a09213665db7ee4cde6e40493bee00b3e26f8f51df1215cd03024057af3c760f5d96a5b65fafbd72585800d520f1ca8fdebe3ec0102c413f5d34e7ce882278925b433b43cb7521ad3a5e30803354ab3bc1ade3fada1f74d4db2e2f7fc64403fca153d67cc9e695643841704f81d6142f42a58fe3d555d723d7eb028415812ff2341e125c073aad767ecd70c66508d9ad17314f95c6675935fab095107644c7575ed4488f86b1c16f7059702ac1cd805ffbac4859e5dc1aeae39e726894a77110405957aca382c3f7c5632024369f2731efc38334096281f90a30620e623c365df9f660b825c8b69e880bd09b4da28085cd89b5d24bb9caaf0e752d63c7932f7936b1bd90483ed7d799c1a0e4cc319873a8356269727c6118ddf3cc0b6844491104006306d220f4093b06a00b4cc912da856b147dfdb94f0fb8f59825a6503b6f9559658b1a1128899bda60fea8b9717925f6be273f12734b2454243b946b6b8d20094c38cfa88cede5cae0732db5668d500dd202355581f1f664179fe07457dda71ddb8f7e66a8537a4df0002d307562bf6530ad970b3c90c44d3628a2a457a3f0a450cee6d637c5670e203ead4cf4d1e1c25f665d05c93312d0fdfbfc4ccbd8b1eb130f60cad99ad25c9059763877cef82e25f9dcc81b31f43ff8a10602a578e9178ead8e101a226f7860c5302ce474f0e8acb735607bd64acdc139323a0a6210676c40cb9bac18803a901dae86957c95ebae2803559b30a90b91bc6974f197eac6fc0b39bfe41830e9e0d1e1540b13398ef94b05fdfcaf60df447a6503c322ed25178858347344a34dc0cb9750c1d6b7f635d3e4443b7a2d3a2a18cd6939b3145060a37dbe5bccc09d30b2e5e3053ca0c26885ebc44d8e078a04c1e39a500b08ef8fd6440f278f7e89a0a682dcb29c359de7438aad0de34d328212e708c0e53c8b3f6c33fb11c2375610bf6b3a4d180485a99b23aaf60fbfe135b16c94230d32a5c728db87a4263cac101a22af9164e8ee9fa36e5491e4006dddb27797b12e76b832b0b5e466f57ff3105743abe6eafab353018a2c7878194c69e5a5d42cb93fb1a58e75f3948d6f9dc0c4918168087214e1b6835a99e4feedaf1de3441877722bfb61337f9e33de4da0d8c4722d1a30fd2ec1deb2352eb4207ead63d452adaff44fc1a29113b7a7187036ebf77ef9f1a9ac833194e60b702bf30ddac40047c24364f510a03a58f0ebf048c60f7d276bbde0e88d08c1a9675f57785d40fc20f89231f7104f1b0ffffc90431d70b0953874365ee20f2e74ad30530caace9af0812fb55493226845a82ff0eb975edc85bb33329e91d8f6f7de41f196deb2ae6162fd64f8a65bcde6f1bc30573b1e8279fd19920c6d8a8da4d6574cd82a8e33a77fe43c86f830ec4fa3fd20e28056df913513cc527026a1f6a374f8c9b92884f38acc9d2b4116ac18eeeae013e9581593d3a34b509944a48bdebec74c39e70fa18d6b0c96513e1cdf94fae0f11742f666db6c2408a026d102f6f4f57f5398fb2e58ea00c7589ecbe36935806a8be61651c2f60306f89b08b45bfc17b6ad59d0ae948b5ce4054dd8ea5478809376dcdc6d3b6be57cece8f744bd30eded7a8578daf08b460400f5f29b47b4191c9be775deb1a2b26d8c0d5bf552c77834dababdec3b3aef6c2b5cbe422f38f6e");


        //tx hash 932768971c60ff6c7c0ccda5e57a1b9f3cce0ccdf068e754994c7dab9deeeb9d
        // second tx which has payment_id8 for us
        txs_hex.emplace_back("020001020029c7b901df84019745ce67dc158a9304af02b17a97be01cb51a00b8d07db77c512d139cd0cd9179633d850fc9b01ca549e25eba301eb02be74810bf65eb30ed033e350daf701a711e87d9c1251bfcd01f007e226f02a821ed15518fff454382da01b4420cd666dc5f3101314537de00d62042206d4b393bf918a020002cf17a9eb628b54df1f79b400b3cf7bfa6072478ac6ac44ae15be52017a49031100026e9a8ac05e10ab205195790d2d05b33fe1e36d5b211531c40bc4f0a5fc19e4e42c020901d909623975fcf81d01e96b5a97669e10e3efbde814f63b10588171cdaa13987610ea298c17ac8f364f0180e8e0c6fe1513c543dba62fc7f541ae219abcf6e2cf6fa52e041bb9333454c900e48c974e0dfd0594eddb8ed93a20de36636107a1e0155d56494a31cd400fdf2f616834c70e400a6d51cfb19922981638a965e89072c157b5dc25e05a0d4693ab3385d11806dc80f4dcbe03fe9c2d159373333ae90beef668f5742c5134c2333743e612a905cc24c246b0de24cae2ace5d52cb6cd6466ec7a6a73841e8c9f579090b0426fa7b60355f3410d93d84c4f4982926501bb6fcaf56956fa2b1bd19035224175dd090d2f511ab97154dd07f7ce907bfd1d7c73f3787bab6e709a8a17ac622798750d570bcc95e19476d7f089b7e67206acd9103857b15eaf289689ba2023c7bea70e898867594fac0e4febf98e4639e5bf1feb45787563318668bef10fe817396b09bdc308906223e4d2ed3159f139c7b113ac2c76ba25314bce45e2ab938c91e303e13b217f5b242b52ef4d9abb41c0f43b50b7feb9999f64c96b6c38016755140134a0286c561543833e39b53ad2c416c134261a88dbfa530fa76259fbc2345b027e6af732886fd419fbc28a6f779746ce8f79eb08674fc45875e76d632b887c035721c5698f6f6903e8ec586dbcb68d7c6ac36d2545646f34ce762016a722f2070258c4edf5978ddd5294b36b2811ab5e5a9d79e6bd4db15ba7ba7f498c30b70435d9d137ad675993000a34a96e0447691e837a9ad5ab9fd37a4e11bdbd293c0c016b613862ab81b9f7c4a29d0dfa5545ae0f011b01877e113dc727e0582ef3099d3cf9f74056eb03bbe885ff4d4994ee01068b757387a0297a265ed3cb35110bcdbbec8ebdcd47f2bc05bd23070cda5579a190a212df15334d72409faae2ae0a040e5073d1fcfefab1878d92ee7879e8bcf007f92c927186c023adc68b679c0b4a004d9322440e2bfc6cc3616063b93b441bc7e2ea028132a7decd345fc8ab02f59c99d365d29bf5f29bec86d02b17fc3600f4547f91df1708001c3b4331c1029fca83d6a0ab75fdd2cf5856135ca1660d575b51a056b699e9c5ca484937c1033575288ea0008dd86d6a03db025076314a1cfe28ebc3a272828bf79995b0cf05a32f94b64ecb174b47f8bb91f8a7304a329a67c8be114f13abc24ed5aae8c9054ceda8f31794238540ede464b39a1ab9e9fb9d278c232a205c55cfbb8eee0006794656b220c307df5cf5edb24970d61819d33c4fb6efeda7299c2ee70f48fa0f8594d258f0d9cc16c9fe357fd02e2c1175bc888147dcf07c8a91b1ea8bf75a02f252070b92d2eadd8b7ac237ea8a66b768e2f31c4a19061da90807d565f94003fa405f616ebf9d541a2ce3e8b5c18eb3a9ddf80c042d6d6135ddf3e279f97501f2524d1b1a25882b5f4612f84e53eabbd1584226daaf66d9fd5fabcd8488cf054b897567267384094c45d0f5c43ae469642652edccd67a0b0a592d534a69dd02648cd31fb563370cf52a63ff47082ee743b95f1bb2d67e6e63892e3c64cf6905bf9fe4d2de4b6df986b582a2edea49aa6fb4678f44fc30620183e169414bca00fcce26d766793dea307701cf6314534f822b2ef36ccff91d311aa10b97ee6a054a383f338bd7640aa2f85cc9db3bd09bc8097dc71b7f0c8d4d774b6556db030bd0d331e8ea2a7365db5cd35e056bda04232d87098facc68d74a19e2c9ec88f098c5b2d6e37c712a31f216bb0d6fe90607e1cd2918d41525f260bc08a96dc2a0557f00dd8c1a63bf7436d8550ff99afb03c6c3756c5dabc76824362f2983c6e0af327a0a5cbca0f990d34f1fce6ba7dad83a8af01bc64be30f59bb8328555f30fe49fd4a5a48e6a40ee060050c8738111c63ceb178b2d40cc189ed1b4e362140a2cd64f6a3c949ee420902b56f46a0219946cd577fe1ecb38754bf071f2246a01b0a8ac95c5ffd4754b1e000fcb5c2efd8af77938f4f5f13cac9be5e938996306ef828a22c6df9bdc08bb2a8177b9704e7bc09e8fdc7edf28026f3a7e4d024d04e371c775e9c576dc4aa92aa2a6d42e0a65155f393e1e1a88740356cad03ac80c21878944191241e0d0d1786f6e92406972750a81ae17df9d428a9a1960d80d05d933dd55623065e1d47d5fca4fbbf38b8272d771044e78acef9bdc2f71ab9f07fbfab7b280cd6a5b7593cd3ee3b2899b83832fb395440813e897eee6c4efb307b0f7d87c8338837be982081277b05baeea16f5b89d9de6489c55172b1463060163886001f5531354c6d1040eae509217e1fb0480be75bce1504bc5b3c71e990d0c17558ad388a260d02e602cd904af1ec5ba839200cc5a7213ce03d69e09ea0c795919c2a7e4a1b9b4dd5da08c951fdac2bae207ed99d274611d848bede18e08a8083d5ad0684fa3d89a96f9ba1030990f752fd12f8ac25598803bf6ac681f08b46f2013ae00b514f52d9ed1fbc32f450113d6d1454f9628b3d8ed69b987b302330aefcca7d807fc6b134d0319be0aa2d4fa81dec5cd49efa8a41a9851af190613c29735ca5a43872e6f5cd111a2dbe134f0689ff4d78c9745c94f8f99317d05ea8ff73e7da9727a1804fd10c1548421678d4a6818c6441816228585a105e30b4b93e6f9e3424c2dc430e4298954c6a001cc91f74dafb790d152c4a7e51eaa065db8db2355fa1a18ca5dc2ffe121dbf788afebd504e1ed6b9aa45e4e8662320e430bd34ddbcf4bb5d19b16e0d57286db680c9fab4fdd83460d986600c52c2d0a76c4d85c36c47ef62208830bdac57a0a34a1eb0d553a4d13f468e335202b6d0ff76d3a1f32f7a671d195c326941cb14f1e5bb37a8c325aed1c90486982bb7d0964cf583ba7f91f65d6dc3ae0ccf8c6e6d766ccb3859c7ebfcbc91666072e240df93db082042c3d541666a17e20bf59647f571547100e0c221809b64d88668206840ba79a9b6b00fc67ed3f375a891b2c146d9797f7f1592cfd9d0085359bb702962395fde2acb46014594c1ca1515d47ee00384e223edbf641bc63554fd1ec0142538f74789287ea89f6400679e2ee36bb7dba08b8933034bcb93b1cb64193056b7fb858d9ec391b61c119215b09c7d96b9f76930383c2856827901fc24035067b0029997e8b1c7844b40e9d545a88b1ca8815c3ca68c07eb0fd50e17994ee053a621336d19e0293c30f2f46c9b0938e11a3c6362eceb1993379918702a8bc004e11884a7090c0dcfa05919d3fc002d7b1d415582b62720ca391cd55004c7103a6b77cea3357a7662688c86ccee3b88e242f0238cc801b93d7b3c060af260307781dcb25c0d8f173771343b1e8af2c31e39d64ef00d66ce537b652128e58ed0b64a70d498b830e2d29ff253c7b90434dd35193b8998b8e18219f933a875d5d071fc3bbe2701076fb683d1f887c13d523ec6eed87e83273b56223c38b78e70c0c742c01749dc151ab208506622e7e2423fc0a62d891c80cdfd8c9bedb8f62d50a88ecc254869472ef067676b52245ef2f2607422eba86f362c52d96be0d62e6023977ad7b8ca58cc0462a5a5cc8397fbbd1d72312e4a66adee5fa5d6602fa9807b76c1dcc4c779c8b6d5a9a141eb6d466eab299b6cd2cddd1db21e30e10a86b03641886d5bfc26e6aa9869af4ae511fa43952cc60417b51447cf6204bdab7d60cca71128f6814d86f4178c6f5eb708812594719b3e4ad852d071b4ad313f2830d44f0bac84f42d0d5644170ae20b8f0f320e88218df6563961f294b6bc7a755013a26f655fd561cdccedc38188f5a23d2e094544c7ad468a8c25849ef12f1720cba51cedfbd64283c2aae05c34caae66bdf8b978317f4b5a479a06424de4c9809410c7f33d37e5a7ec1f4f8ea3038ede564cc3e0dc6f44cafcfdd88e55fe0310c20dc3c472e3142c13fe8fd03f9d4ddd50b1295f4fc9d54b3af871cb5eb7198020cfba846b2ab265cdbfb93acd17b5516c8f9d10d115ff5fc32d374a9dfee640f7f696f43713a8b5237f34a190e6208c464b73f35e9c282c0d70b3f308876600c21b5d155a09b54e3a95afb15c3aa44feaefc252ce514081e41b65126a44c640a76b18b453da561c7a8a94f20db1fa69c2ee601df4f705782b9d112825abcf400614113828bcb4f71e560aab55fe8f40eb460109d2e6d3510525663328488b50c5682bece54c314ef2f4dbc123cf1a6784185aa3e73c8dba15410ca2b4a014304fbdb20cad6793589c4e8879e07026144cbc6884d52a0057f209b6bcd0053dc072fd94fa6b2a055f3898d54b4935c35ecb9d89d6d1c91440a4c1590e1941dd8019b19bf690ee8d0814555c9ecacae31613700d9eae5fbc15e2b512f2d30adc0032dd448ba2d4ce49e2a96702b689e3d9e14c0374257171ec5bde7b614d203ee0f91e7b2eb0f8b930b1d7f14ab62455fb1aa2e1b3b87433e4416fac1863d20b30885cd7e3ee8f724346866816b7572ef5c4daa1dc166f496359638fd09dc34dd05fa1224bc8170973ae3d788266fb8f03015b349d8f9336778ade9dda4b27cdf0702100ab01282c6d6749aeaf7cceafa6dbb5c88f2416c54fce9f8247217f3b201c648c2938beaae0e39662b95b5810f0bfcd7d41da0579c88e5cf737181288401f646b48989ea10f49c44329b55bc89164177ffbdc110463fd57ab4d34ac3d5009a0084f911fad9b0c3f03bddbf250b8bf9259ab080e3408755c19f2496c4e602fd506a844413ac3a0d7d7e62cad6a387d28b2f80a34c7d0e34a00671a50f4a07a59555d5f49b131be93219dea7f10bde92aa8ebdba9008c0262917ed46911c0d74cb1af3af66e98f9300e9b9db0c4f73497c41f6543230f37a1317bfc4247e0a5ed8731218c976561f426a92a8e332e03b4f3f4890824933a3afdaf047e45601d92259991efb059b5781e3f797ad664d71357ff80a914ce33163645a2f35890e0a3c865b414cda468583aafa53e9a75996c007100bb8a9b56530fb11db063701b199f2a087e41d4b7ab4c2ca26aa19df42c3a3b4b2cc6e42e4426fe70ca176089022119c3c86f952f6b93c4be98ff14128295c1969d75f91b703bb8d87ec2200da3a0914ba075fdfe4074e5df1043e0fa39e80d82d48d1bf96240fe1ebca4e010d1f56bc7b169561744ebc33c38ae229f6939cc05be94a63477bbbcfff6c1908bf79a71483a1c1afda2a3faedb5031352423412286806cb614a86900d1ba2506aa36b9046ca9f9b92db2571a9434abe999aa9a56593cc1d6d0b769e910480004bd2f973fa8a70c5444b437dddae9b91090c14ef5bccb5c543f1952665723800c8e55d55958b82ec2f54c74fdefe581e80bb24c9d98d2b93137b110c312305103a496445014f3b028535be1c6d1f9935e60bd819d24f42419d383814f2628bf00bd66723493e3777adfee11a14b72f217b4515bd2aa507e9ce8a37fa52920520da2335cc49a7621d1ae50960f1bcdc4c3a7daddfc5b13cd72eddd1654b55be00bd262f44f4d796caa1c6f404d899cd38824d9457b45e4f04ca466b414530f110d6edc88734c6d9e7a585db2355f762a668e303c7bca9f94df539afb56f07ccc0ea25b4d8e258987398b3fc712502b4f13fdf8a2561f896b79628914c3bc9bf60c4701f3cf31a5d5fa725579edd41f93e3eb76ab4e04b49f2ada4d848b5455ca09bd72c127da4e5279346f1224016371c93e646e0b13d409abddc7b6828082aa0441facc9498160709dceedf3b17229ef7fc0d846b806cae38d09a006e43ac8e01f78abc767f1c63d783d3a6ebc6ac4ff7a4ffe3b1e99fe8f666d8597c45fc3b089a8103f2e6c6fb82e8c8bbd88a39b607466f32fe3fb0ee1c2292de5ce13f2d08e58433abb497ee8fdca0a0e344887122dfd5a3ea44b996fa700ceed624350704186a881fc1d6c6026b6c0a3da668b55af66b5d36c43b206056e4ce177003a90fed400f8ae228c9f0dc5fdc1247546351f732e678b680c7185e569e47b491670c607188e1e2f40eb7e0bf2ee9d435d641a16c9d105f8c8a23cd9845da16b4620b66dd27a726ab7eb940c954fbfb3898800ac6e9f90f9472162e32b8dd51e9af0a6b49346aec4eea31d7efaba872df830a631c77a6003d3424d1b3a4b291d70608f359c348e68abbca9ab8653d4a75396e98f2255bde2008340692d9f466ba7d01206f97caabd6b8a67f16b95ce38bfac02e5d009c4410fee10b5cae079cd5c8f41a95a72971a60daf5b6039c76d625dfa8832cd2a98e80a63c6a394023fa7af2b82c625c6b56d4ef011b300722076e1eb3eb88b10c769ce0a25cc54b6630077bcc0b09954b536e39cfd76bf6428f6ad7bab1184be817844911f881714703164e9427efa93632487a0307a951154bf6b791ab10204a07d6d592d81e3f78229ca8fb21e88c7b5ac54145512bf9832f9aafdb6439d7e9ef7558a9e1779686c44cdb76084fc211960d669a50ffbc43d54b2f6ab10fcdccfc2b847445824ba76f58434666f60b386842780777448a065fb440668946a60320a5fbc6bb7ba56ce16ac3bb7301b495f70a21f9be31a3b2d26b8cd11b93d40a1f5e9ca9a23b9e6c83d9f3c3236e7d73c9ae3030cbc5776c7094658084f1d1aa31a2f161896be004175048ff03c85bd9b8799f3ade61f54b40f4dae958f6bab35047151c6e3be7efd40ef29274e0156973bdf5f2b2c25c3bdd03116d50d9d01fb26a2e209fadb9ba6bb309c74b2e3bf5840819c2727686dc1aeafd5e69a58d298d2dff9ea2b3ce525afcb11b0dceb3a115789defd7b6cafe0b08d50a41fe42bda3f5acf5e816ecc162f31ab7bd343ff5a0d23538b2da8121dded6c0a9c5770590422e414be4673b34e9f1767167a7d2aa1c8d7ff3336dd518825d7208a5a8ac466fbf69290a044a34b711fbdb21e2d3c8522a47f9716934a1210b483e60c594ab8cd6e4800b0ef50b4325cf471c95c2e39fc82f7b0a8bb31cf1974f6386e9ebe3f0bfbd4866b3af53666b79ec743c4fcf78d715ff750699e6e199bf05f9fd5fbccf99dff8fa177f809dfd26a2ab7debae0c98f9f871144a6d4942e1d9d446584bfa407281ad40e20ccf6c6b76296f24c7a88c8b87ec0fc5e0478e9517faab5fecc6abaaaf21daa320a696cd7f2554862215f4b0dbd2b2da75f0814f3ff42754fffb17699528a6acbf6732dedfc44057ed0acb21ddd2c6d504f9d0949d2b6fc0fcd2d0a8d71099bc2bb5a5f25eb8d08c3989df205db72105972259b021bc62ffe0df1db12e89e27d96ddbab14bfb1c443cc17d9d7763c824fb04c7560a0c00fa74ba7b374360b626396d68ac4bcbd882aabaa545f907351d5bab92081f95486acc7df3c0f385950d4ffcaf9ca1f1cc38634cb698adc6bac921a205dc371fd33599b818dd8d948eb27a939d2ac8beb4bfade2eadf28f4b8f3c4e00e08609c0e1c9318ebe940e55493fd5c50bf95ea140223c730f21389d094cfbbedd06e37154b5f511d87b02ecf7ea500192e77609fd49d6c7d7e5f7c15551f8fb7dfa8b84b88a9c6308090c321255353f5b33fe7852ad66d9dc0b4ea53f35d0a6ddcc924467cffe19e9380ddfab8de455d41c416d4d21b1524fc14c1b42c01136fce6a781bca7fe62d4689cffe55d2e8afe65cf5ce4bf21e295ee3f27a2fa9db8c7025dc14462dd493198bb51b6d7bf193d0a6a9c88c9b59b65b7cc1ceced32e0aa962ac49e067e16a80967281983bec2d9cd54dc8f9a5fd20009473a870cb05580125ac7146e6cfa378a702b5a4da369675c6a72b6bad04517a7a6cb6b2e063bf08db6737fe38cbcff9b00bd480ba64eb19457568d5ee461ff4d4cd2ce1af09c0bed78e98bcfe43c289d0b26a989deeea1ab1443ff1a1dbcf05e1001245a0fdbd49d65ec5ef91604ed69a288d06e7061ac1e3b290e10d2d425ad5d8cb7cdc78b0c704b72516e36d9786867b39e9fec82df3dfd112f392cb6cc8be1662579a4e1e1ee96991adff1178f647e969ae5376804d68fb7927fe968a0c9626466f8192e5986b1a95a56caa054483b86d53f0fb418b9ce0e6852dc1c231efd93e4aa95954f008d00201c6720ee6ba0f5eaf8a93abc66092f01c1548a87901d927715d2a36ec57fb8ecab6da04304336da2c4ab8ff02b1fd4ddb7a3ab2d6d5d1af87b1dc0014895a0aa4f24b4a840929d65ea3b3bf42bb219bcfa4c4a6cac9960b35ad6f95fa3cc012858476e04a6f546956d34316306a5d5d635253d6742b6a2d2f1b63b6863b52948f81cd1eac2dace6591ed0ff09e8029423e87054f68eee3cfee17331dea1e60815f5f78358d891f5bbaaf6458981793a3752552fff143ea9155f523a7f1650fd5115a8d54c513160e5d825633826b4efe51d2fa2fbd12ac1d84c16773aa22906bfae907b916629879527a98c22322b17c0988bdf2ab9e6bd2f45ec6a5947e12c75ac4ad7efe89eb8f614609c4d454f12aea2e8b13c17d27d6fe90d9c9620328d70d31d6115cb53cc7a9cae8b9fe5ce320fdc118e1ec79a89f3dc273a0203a806f7d07fbeef5e3fafd3a00535af46ea5b3cfbde25b4a593bc69e62a0232977743fde4288be2e2b43f489cf11c24b8cb18858c6cdd4c8f0b0238e1e56f9afcd2d0994f758a5f348906801dc6f95873c039835a879899567a35da8a1f81cd00d8ffe045896652f1e223e2935be2aad16937262a548e07496b616ceb8cd84a05bfcff660b1dc3e2e9623a2eee4bee8d1fba18c564538d55b20e616a3e854df999e1ab27281705451babfac3fb7022ea2254d9e05a20ca024dd0ee6c0daf36c31a8979f41e1375ea530bae477e5a7ac8a3737228adccd0457e6f1e4114c54f3847e25155aea1ca5ee0a9a1816d4a97093f58f80abe6f81a90f6c1880c5a9cfe3d7c4cd9182bfc32be3714b23e61defb6e48fe321ea294798c9a84fbaea2a986f41daa27c0cf35dc435631d085d628bda88b3e714698c0a63f93f03c09cffd2a765402147da8679d63018df513961e9cdcbf8d7772d1ebb10e9a9bdad882fa318e029de5a9e83ba8e71b3bb48b792b095ff9b1881ef30e6b6741064b80179be33c2432985fd31dc8ec93af0e6cc3a72629cfbba6142e99e3d85d142b96a961c46f456f8a9c0f8218070ae0c5225d9f00115b4bf166f6207f74704e3ab11ee6d575703e174e2876521b3fcbee95c7f80c06d7f6e5f20b727e4be37e5b65d231c39d1726cfaa918f4f53f1d64cc16e00088c1a56882640c4cef0085cbf586cce339f4ac2b249d3397e4cfa00193dbf82065c56901d57590d5a53b2d615340574db19ce3825bd455ccd4f0c3a15b410570bd1ea73bc6704209e9a17271c732c1c84a0f36871ee29e06568879be2c2748f025ab9788f1ba7d640afb30273527619530de46d796008005cebc6f83babb8650e3c0af30c1dc6fbc6114e08b0b07fe7b17e092d35118158c89a88b1c52fb03e0f1a3cd7676722239ac21d2a6b746aca0ee247a96fdb861a4932d195900942c70f2f37a0a181a74baf6e56b362b64e41602557fabe5f2306e8abe1e938da41c2044e0c81b74383b46350ef6095ff48808f32548cc53270372117e07202fe9f5506899540fcf2d8042b938cd354439d5f908c232eaf1a5e26df27b5b6931fdd6608afd345880652bf66fa45c325e4160f4c72ec85b9d27e4ef990b85f3ae05862022d454fbf542ed277f95fb40a447ef49b8c2234e8d3a89b978eb51b0eb0566e01d7c5a4af85ab06cbedce146efb17f1293cd254107000ec15e5592c0dce386a0e519a782a2f145cc4a7f5f75ed2615d01f8ddfaaad2421f920a33c06d0a92bb0cf3570adaaa5ca2117d23672a98fb75afcdb88804ad4165401ac7ff9912fd5d01292532dcfeb739912a7f0957e49f0f133063d886e571948acd3d0017f5b89e0aa6d1b4e629bda2e861f4c7a968c8fe483835cec898aba08e95f796c3197f710c93e90d7b33c5fdc2cf9e96179d73e4490b133760affaf4a166fc3d524894bd0018ff6dcbbb34555f162571b2ab555b3691cb2e0ef5f85fdbc5cc0adb5aba1c058ca2066c2c797eff59dfb7f06e1b8158e0a65b646ed2cae51c776840acb1ae0b81cfeef780654048a4a366a5f932ca964e277d9def7c8328c2789c9921e98f0fb595bcca4e3d5fc7d3017f000d5a2121b29ed448e96688d785b3e78cb75aef0749f015d8baacc65817e45caa4f6e305084ec9aba364833bb78933077c6df400b544a900c4e4e36a09b4cbc09ed4a8fcbea56492547f0e6bd986e0599c90574056256abe10b2eb0bd64d75c12fa4d44bd8d783ab7c1089ca2abb844cce4381f051928ebb20fc7e99d8dc4fc2d941c5b7b6cfda08a940a9307c345390ed8c5380908595912ab0bbfd78026e9b311b945792afb135308d5f5a5c9ad3ecc1bc23d04823b5d435f3b926e8f02b05cfa1fbfdac0ed907de3747e683c055424122f40027bbeb42306078a5a85658da3c1a6e260c25e3fc3eab0d8a3c1e5aeb32fcfb1090f8766fe19a35dc8b0bb0c18307d1f522d170f7a8063c7829e1742ead1d59404819a6994462f6747c3d5417082a564cf6581fcc2320c5279e8fd0817da591a0f0f0c2dfa9c83480da2a34f3afbc1e61eb08eb0fbd3cb9c5088ef0a245726ca0681b16399ddef81d04874577d7c61cef6bc7488cd4270ef64d200b24fba9b06075711aa7339183a7124d455bc5ec940832fdb684d107cebb56b74cba25501500ede8f1289a9478294947e450ddff911e9168201cb33ce2ab8d0c6766443c6a30c63a29d9608277fc29a07fa664935675703381016c502e0d9d4ba987b975f6b0244d9033e5ab34ec18915e58079d67e69fb52438e86ff43e4ef23d1fd8a035a00f945900aea15f2db8e65fec268ad9974084edb5e84930b0c2c0fcdc4e963090c1b0127ac7b4d0d78c99fea25d4b9f1fd2532ade75803d9b442d945ca9b9267019e4d80363e8c77a8288822618ded9f16f5c8d2745c0cc3da04fde80265ff460913136c7c95fc940e7b52ec1b81f746e596fcb20852533d4d8fbc51cea474ac068293a104f2331b3c85039c9f251996ff4c112434487bfc5850ef525349e3c10e676ae7edcea48faea77ab5c2784b46637dcae6b13c5eab4d4ae756b25d40b0049a457717e5ae05a83e401fdcddf82fd90e3ec8872394560e03b53a4ad3efb40acf9bf4d949f23d14b7dd141576770393e3b87b06d2fad185b26dcfaf54f9370d108a42f4dc5156337b7694691ee7a45971c21bcc80ba0a05217038425080820fc6e88854804b7e13578053e14f6585ecbc16912490d2df316855de093c022c07d4c4b2b5e52d7d024abf791414029b17feb6a01123538d094bbbe073d584e603c836067a916d48671e12fa68de62758dca2836d0420a57963ff9e0598004f7065d3b04f243d0e4e00b1c69eb3e29a972e5a87fd85868594dcf5aa55a8054b20544f715b712eede43f117cbe8d109986918222d1c0ffaa34dc3c96f46cfe7480d6bc091b3e1b37321ed6373aaa48aa5ff7161d7ff5a1f3005bb1c2c183bb116080d95a467277ff9021f9bc9a47cafc24ef448a7649729afea5166281a8ecb8b0c3baaad6eff280f5a0853488ad493d6d1631ab4baa9c0448f6a1fa0410f6d240721ee95d8b19a8a39e4da3806a047a7c58882132dd47e5c39549f6a33273d870289a407c31abc815122debda4418ac88108e3a4608cfb3f93c39d6071c274bf0f110f09d2f4cd88baece61da6f3611b0abef712d9767c029fb78363e9a8d674000f0995755c9ace197b342a0874c59d66174dd6e851eaf86e8635f694928d750eb7a21a3d064d4efe406338ed7188a88198deb1aae32cf1fef8c0a9b427e94206f4c1de64631cf6fe551d50732a08b5d7e3f3516cd5932573707b9564dbe40403387b818d02253dae2c1d9cae74e25c809f1be586e5192449b5dc1633c9fcd502c02dd7da5cf991f32e1575adf88d74830e21e4dce672e815c0e82f472cc45e05b2fa8348fa9233cc3fe7ddae252cc3be768ec97b07352aed876a5cee991efe0e8aa88c82f03311d0222886aa72b3451954bbf20ee87bea3e3b302961c949e10abeea87eba50e82b5687c36824ef3e35892e198021ead87878af815c2b0cb930360a1d206a8752f233fce66e0106a18cee5d6e5eda7f70247403633a670924908d65e66904fac57b2ed3e985d40c40c0340f6fb05bd5dd4912541d0166e9d8b0482a543408e8918a7b517b3110a16e01678a495c2f646f1e9eeec4404d447e002f7cfb61a3e05247bdd9482bc2f7e276c112f46c9c67c73a1bdeaaf63b174060b0af3ff7aeaaf0e7da7d3af725d3c757460831b9430b4041b3e30dc2487ccdb0b62f1cea34ffa3acc727ade20fcc12da9c0f16dc942acdbf4f292c1c343572e0f51e64c204b953738951c0500d2e30c7921eedb9470dff349dafae73f7813a60c2408cdd0e042105d03495aa27e76fc988c892406c61ef12d1f3d16f979010a0d443b49c62b4881a40bfe8035feaa6ace096080053af7ef8f5e484c13ac5ffe0000626da9ed0e2c9efd12bdbd6ae9399c3e2dd10e1d17c68a24e7684ce4250f0a8417a5e311f348ff8c90c4f5f65f74ce1789d483d10f12f02b0e4febcce81f0a8873890e633bf5572dc9db44b541ccdba4ac6295e77f582f114628389ab9200996b2401e791cb1af42ec4d7bc9c0b8fe6906ce825c1842338d06777bdd68d40d43560b55100d98c5381388cfa5df90fdf78b3dec2d16f9bb341611320a34b300acc35803c0dac668441170f9f6f3a2ae874d5af74d9bb82c7ded78b1bb76ae0f71ea5d7fdcebd038381414ba61ee75ca8f0e2d63509b109d9b76dd676aa209028b242e8d94d099217558b3d5c2f5794ec8cc5d2cd712d00520d92e7ca384050b5d0374cd013135b07857c050b74c113d4a30119578ad58ebf39167cd6d301b0a77ff9f91d7673a0fb2af4154dd8b55af5f92ca949c2aca1fe6b9e972f750360c086fb293174ce272bdcb0b4d0c09d7da9ad31f12635c53ef261646ce2517a608414c6cc419a8e757c50d8e360fdbd31ca879cd7e272b65ffa09f69d00360b0091c1d2294876c39cf90283a09c85f402ffdc6e21d589ee4048d40acfde616af06722c82e6378424751801f1e9418985e24667e92f64dcd89414f6888b884794021be9d5afc6b5e148213145ed1ce5b7ddedad619f548ad3ca7d7ef1a29afb660a9a3fc739d13956b012cb053251c35236149212acc3a89512530010e17321810718a461915046aab8d894e867153bbcf5a714b83b572027688c145bc06e90c40fda11bfc5fa9b8c7a5ef91ee5c99d5567c31bff3927810e38a7498ee9db89b108480847b238ff8a42885b2715801f16c030024193494187d4a4e8a4e76703a20f7f23aaeb8b70300fb3988e3407109c3a3265a23b08768f2516b3aed9204d32017059ed7e57386c389ba43f1d20a91a4cb45df3447f61d89f1ec25250f7d9280ebf4a8c6d8896c086e04e478cbba920b72e144ee62086dac5e9164121f3cd9d0ed553df4bd9970d4ea26d33f0545f9c52617817b3334abab7ba839a4485f6fd0a6512d901fb87842befe3679d5da6885649c4544340c4d95c77632a99ef5a2509318371dbc3d078824c46a192a89bac08303a11e3f28a0a3d0be42b4dd332e700559d44c9ff55a0afce733afc21c97005c12c386c45c062f139f56800366f5c0196da6b928001647f992b296fd7b67f6d1436a3f64e226585356e35aff9c630096597e8b99d41b7023026d12b62343c587761e69e04ef0ef6494ee7d93a5d56042910d16a5493353706962d670c3d4e0309162e64e78b1a4a1d0df64cb7c6ab0be9c9cabba09842ca94b2aae7abf28330e82eb02f29dc83af30f866d83e7eaa0015d0d77916b02c7090e236d97bb62d066cc3d4f84709b079c74e2c2c2dd8080b8688dc50664791dc4eb58612c408a707530a902859d40a78e17aa5510a4da501f91e88ac4202e397a2ad4a4526a0f363c328b8843dbed047ce85c92c06423307ba29f52df828cb7802c69cfc127a2ced241338714df17ae8c70e5d22b3fcef0ad587d0db22221693504d44096c06b0a6ee44bf7788b3a24e9c7bb7ceafe10b0e1cef3d41116dde6e641b518859a792e71b180131427e537b767a956138bb260926f2e2e7d22aee1583b19891f8426af63dc5abf929a77a672db1643f239c9b02804045447139f3e213a289043987988db5aef7b3e15c8d82f1918672b82eac00631b828b3b59e8cdb7f64cff309cbcb995eedb8594c4fc73aff4d36c2201eb05466bec6f5e2ac6b8dcc78aa56703edff81d7a998a6353ee8c553ae55116ccc0d1b12b8701132a534b37ecf28d1acaebb08239bf42a8bb28b667ec441227007021d99284bbce70287aba20a3e1b24868063a280c5dedc7758fbd29d161a354b08d4d6ae4f3b76d4e777b7a5bbfdb6dfb8dcbdac29d4b67e7ec857b3093a91de035152ca5c955d08aea1bb125ef9b283f65365972e7e8817dd722dad9ca0bc510e8cc2361e7d4d21f16ab0668082eb2c66c96a1911c5bd95e32a3a48a7493cfc0607f478797c1b20ea77d6ea7d1558479e4e3ff95b8f6f29cd3742bace27a9a20991c5e1fa8e8c3f3d0b9e080c5e55d4cb100f90f7af7e0f7e612a4ccb4a639208494e458fc5b98691ede38662e6a757a5d6d23df5c64ca42bf3d91dd8771f4f0ad90d9c527908a11b90830f978f9b28d78b115fe1b35149400ba0b7dbee3dee02ea3f382da9b6f51f7f684d60ab6fdadbef14f19dd728f86cfe85b53299708c00ca1a809b46d4d6c3dc630d5fdf48ad9c1bbf921ffea10260c6d951fb896632011dc898826f1b67e7b5f8949787f9b31911be17e9ada5352c1e8fdc4d34c97b05b15b49327e681992a3a35230c1dc08746ee2cffd3d06e1a63f258308899bd90d9ceb1d16c105ebedb6aa79acfbd054a8e1b823dc6e130584eaf3ba2bb1b514466bacbb712b8ff56d51d4aaad1bda5d73d1ec9d7d921eb01296451201724597c446b7d8fc7086a67e05d293d680a78e4e815da14d33960318a384b55c7b1b95d19224b809bc922bc30287b106094273f9269c2b62a70711c7471d2c451440e1ee785f2884dc5ac70114c67a1606ec97ecf10cd3eb70f1bf83cec3d64b3334d9940bd72d5f7d1b75e4e8118804ec5901094deef1e55d6bd5a1d866a38799573cbf288eab4b53483cdc69a1b8b47de41cab1199448b81f46ea8dac3e401a804579d0a89275d9c539b2db699352e7f21a2addc4b4bd6eaa6fc85f7254d1ad81d88ac9e4d3d90b709c00ee78295b9807667ea338c1fefe216011b7ebca28fcec2596564bbbdc93d1db4ba887ed50806364a5ac029a0aacff6bb9b137003c9e6c601443f8399fd0a0bcfe30ed88ef5ca245a0c323aae1709a51b66595968dd97674caa43c3b82ae638a21cf0ce95c6e27f8e4a921f56bc1c5f4ba2538e8095cde86c30b9baac877ea11b1d6f67ef0870c26bfd7b37e3fa084ee12d68947a44d65cd7ec90de3f3e27b9b9f3056db9ada416ff69dbde3d722a1506768238b77af0fcf063f85daf8b5b0dd179754b0ed239c61391627647aa477acb2a21dadc0bd3278c7b88f83925bf07f2fc240c0048b06fe6b009fc5200e0a0bfaf5f8d9ee0b4d1cb1300b51c628494e3b5eb2153c50f8373fc1680097e02d7b668fb863e6b1103e0a6240de52fcaee4cf1434ed581ea7b0ab4543972a3867ce49ae8e0bb8f2d767045326acac1223e7f56013c5e3dbfcbd69604284c870943c2c751744e8e572047190b9abf318be02e4d9d61730339d615800f7e8f60da4bf655c36d039efa2c32323eb7225ca10685276a7c7e4f248e0bbb6c972598e6c7781e68789fba1436e8ca2227dc60a08fa29ac51edaf603224b7b47054a2dd34ef486211a67a76465532731371d5e93b1807e98e10f4bbf6e55ef5fead6904e609827cbb52df015274a2a1efa66585b30acbab307ba08c34817b840e0d26ace8be8eb59526b882d506e4742b6059e8f82b4b5c348e4e43bead06babc9de839686258647cfa8ab4c93cd04d62c9a60b5408222292adbc71b2fd73b328f104221d8bbaa464b66464cf049b73911be731a24ec884722a244d5806bf60d566960a06f43dbf9aa59042a9bc6bda18cc599e7f2e6a8ea54833fd8a5250619f4cb20adf490c28f2ef2d5862ebc2587d4cd46bdc23179f9188623b5474af87cb58864ec0a33da776a8d951508e1a94ffc9ed2bfa1c1142b576ff778ce99edbc00a14826d527c4ce0c40aa570a5402aabe37dae711de7043d08213c77beaff22c834b3d76f08771a783b2f0adcd3711c0f40f3908465f19cf866e2bc834f8d4ba9ff14a2734dcf52d3d63415d37119f994eceab65a630ee05d9f74f49b2bd1174f18201a38a139d01577003221b120a3972d3fc7c5f5d170f96935772fa39c13e1946175cf6eab6d4fdd6f06e2c37e1e283565d6a1b356769672ef3f74be988532847e4adf26e4af5dfcc017d062dd1335be911cf093ba5572d1c746694ff0cb4904509a08c0d5071092b60e5cb74f60175a7780c3cbd29f9a43e76e2342ad54ec4fc6159b1c9ed7537bdf9dc013449c49061b5faa79f72e97ac244b62d024e34af59ab26add04d38833042017e8eca8353798cf4d865b08c15e360e788e3f11a459eb4406a9e58ac4ab917ec0d3a6ef6e51a6fa75896b93260efb16d3a28d4321e5536363d732af5ef72aeb242dac42b3e80a8b0f27b758c3b4e3325cf27158c2871400af13450bcfa1af6838a84a0671caffbe17e9dc7db0323c0fe204d7c2d30151e0c629c83690c15f0639fc6e17898ca89eb2acad745db0a00d8959e8a2b9b6cc4fe6d99c69f1bf87acf5394b525a673f86b5e26cfc5564493a7d5288254fe595169e1160fa2381b710f35478eedd02644a0917409af30e664172944ca3163de0582202fa8a5d0d37b9ccbb264642b222825cff112846f74d4da74c27179df24cb272d52b3d211d92e08d0034d34575d1f8b2abc0510e65f3465570cc8f4bec7f916490f4acaf6dbd391f73130d6a2499992cd8c68fb29bdfabe89d69ca66dbf784570072bc3cb2913c1140c22e6c12cd878eed89e6ab6282219a033b7e86d115f4abd27e7d1fd7a25ff0d06294c9d9962b09d6364f611e8e439f8b4770d4e0adb41c15b7c49585d6f44307943b3de079c42924aa3fae4f9e434517db236ec3667fc766fd671511c52e30f31dc048caf8d7ebbfe1f706aa2f6c6b3d0b944e155e25fd1b256df1b7333886116d107d6e911e95b0f7dc77460707618ae0fa7220c4c88395cebc05f4971258168c45ffa17613f8e43427bea491611bf7b6fbe9745c24e4ef2faff8dbb94564306f9938c4b1f4e3e26a3170926189bf3bb9d745b2da459dd7257dd5da1914cd32c0881e064140578934409e6bf0d6689cf0e0ca8ce0cf90f515b974cfd3df1cccdb4423ec44fccd6fdbd0ad7cc378b6115760e23cf185828cdccf3d4eae9454438533f8871d47b0dfd5bbb9f5d6cd777fb9b811b38734b7ca11ff66d07f47be179f359e0f0e9a4fe63563bbb753103e2d40e09a75778bb7661bce7b4453c1a5d817dcbb15ab9e57c8ecbb64944f34924946356849ae2e144b7006d29a3c45146dd3a5ee8dad3fb32737d655e169b5f70ec10048065113d58d5c3d14262ed6d1a46ff6dac41973fe266af24ea6802ef8a16aed7b92955f05dc621a0b9f48eaf4b7f76072e59e7efe4a097429f8aa155b12f5f686af374669011e42ac8dea0e3b2d8f513502718b3ec252546cfa64a662021f4882e20cbf1be7c41fe9ba9632b8c6d45165915fed96c55a44aa31ef2262182cf5a6d82517ccb21a019dd2473fcf8809c1201a4311978a8b5e274ffd30ba98dad8883350ec3bd1e7fcd3234466e51605354e2d4b10fe75c77d79cbb9f21dd1ba6c296e7c3ad454d4976f256cc86824006c3f595cf0ce2f1b95e4ac58064f588873854e7fd5eb343955116b891bb26d01a188131e9e099e2fe1f29b6253b08745398d92d88b3559be6d61e95207e1e201efad1d4b1c47557f868c8efd1397a7d8c4db1b72a14ef93226b47981acf26d047a07a972b5fcb8e5ac773be54988a18d6234b7fea32907b4518cd3328010c80793a1786bccb4ceacea50db3dd2a174ffbe09062f5a52b499e8a2dcb41eef2f037f9b3537348bab257fa96ebb1c77ca6c4e7f4e1445f072fdf802562caa2e770a72821452aec272db5d086561551b6e48ac2f26664cd582c44c7ad1a3aa8bcc02391bf1cf38f147a36e1108b1303f8f19db0b1f59de9be87a92ce47267377ef0203b3b11099d0f2159559f4ed12fde110d52d6212e37fe997cdf89e28c0dbd6046fd30cae65a6495dcd1d3b9a0275620efd7a608cf9a5b2c0ae61499e97c994023093a214490b438aab34ae372e3dc588f21f3ca9ad480a6c8aefa87ce81abe0cdcecb9a863d7f158d130a03ebac2e7207fd559addeb600a316f885660e0df406b182ab53925ada1ddd85a1cd1e8d0df655984d33d8b7a3e59b0a957491c92f045e2e65fb2a54945e388bd7d6ff2cd9d0cdc1cf6cefe3b5ffa51479db4545490cb0e4bdd101247fd89444e44e1509a037f754186492fba7df79f69b477c2ec508aaa2563d6a8618a8cc77848c9c59d61df48288a229def76b99164ac445f204025b9bbdf10659f10729494666b890a313222d104ced5fa81b238229c26278b705303bb57528ac910bf72110b6f2d63c729324dd2bdda657fcf6e29dc5b502a90a7d458b485278f1855339211feca11b092ee5aff487b671cd0bdce950e44f15075c1bb82565d76d206e9794f5dd6e3bde66ff526af40663cf58df22205b88c60f8adf4a22f5218dca2229f8da281b6da0b61529ee0e698f92f55319343b036a08a4e1f4587e59d9129d5a860ba2ca72755560ecd18359493f9c1f3be4e386e70ca655d21f0e12841c276df8f8505c8b2fbf055a6c9f64a713a35586eb3f666907e7e546df3055ea58304b169033939a0e07f499c2fc2b26975bc3a5c04f7f570e9eb652f3dedee003e48f36d7f459a005a09f047a078596ed93570e1856ad7108c836361dccff698288e16feed4d93af5847e519fcfc3c25f92315ce9a89ca80daa71555ab15a180399baa7af4532df66f3493b9c186f2e6a9f915cab38be320799ecf73821f89741215438794d7ab36e5503c4b345231ae770f895c98b1e41021bd6a644b25fca64879bf9586906992bc79f18085fa42a7e72921a330020880b3a8317561f9af1ac204b7086010d6506f27c96b3d0948a240be52b74a0b8e800eacd4eba1e093c8ae558ce2a60cd33fe59bb810a9b7fb17f5a52fb4636b56a0b5fd29ef338780cfbdb7a905e09529eda2b456849dda113214aae7dfb683d930ccbda8983d205123907a84ffe0b5d21f33c7cdc74b08d7ed909f16324658a14026d9e9692404f83447a5dc7ce32b417052760c40faf40f5bdca971c30740c87097ffb4ab6fe8366a6e354da21549d4b67f04fd93e64b153cdef5a78b8ecb56a0ad476ae9ddc457e37ec247c00d888815c0171e1e3e2159f25668ad07a1c227907481ee7bcea43697baf0797d93bb879c1f903f9414f77fd71fdb9f2ede9d1e60c0ef5441a5fec44a64a619711c7c9f74909229438ed8c4612aa3078cec428c80db510ff870900c1aa7212b65f9dea729f997e6a000cb379f656c953c6100e3109c4d3c82ea020b1faeb0cbc62ee7303f90682ef208ea4be7f05773b35cd26ac02a49a1a26c19313d42640ce2923bc7558e7e07e7280aea2e643bea4ad07b9030e6d7073883e311688b04cae0310fb16a27f180fb35f5beee55d3d9336bf9a1a0174e8390afb26b242a844cb46ff74c57993afe362d0a045eb00738f1a8d4a9f0dd6ce6da9afd5d3919d7975a65cfea1049f18477284ee375f846458d961b0580e5b42abb795c931fb08c4733a6b17bcb4d36af81bcf949510fc146d3d86acbc0e31e234a3675e89f8a580cb88813e115f24fa496c519cf43121aa7f16ce27fe07a0ad088f96c2fbbc973561d758b093eabe616ec34f7e825930e1adade32ff10ad15d27743984226da8d68c54f29b277e21d2cafba24b6222825e66a2d79a300848ab8b76445d8d6869bf781c5969384e413afe7ce8dce44008a0bc699c277106335d51e1d7e3209aef503aaefadc00ba444c90a654bc87293297df676812d5090ceba7cf66d564a2905a71884ef64f1aae3cea1c08639211ed07295f57e2da0f5c4962ca586982dd4dd8650f7367d7ccfb877f43318d73e84b5f97afadcc1e0abafe943dd2a74b8f6f50a2a2a335253d910353e7b2a5392eba202943a4f40c0c0f7f7bac957b49f9fe872384505c847a60b3ad36f4ccbbc33182fc2084ccb806abc03860826b9cdea7e3e27771c36f05e9e17724a33db36b77ccd70fb31cf30cd09a3e0eee6e8bd20292cd8219fcaf9a18fa65f2c710d4c8b86329b5ce605e0b94cc88758e838a9b2fcfd67375412d0bab34d1621f9517d1c96d9bba3406d70df26fa88954c53683934d6f5c6aef11b2611c9ea97b7e6ef386f3b20eb655d00f2df3b80cae4ef5525b675a9072c4aec6301ba61e47214e4d1a5a4c797809df0f54fea3d68de552c74565345bd17af5f3257efd07b2267a1792d27e66a329df09e4f47c016845ade51bb00c044d7c838ed230849b807738b8759b0457d1a4db04188204c6563784db11be160de120364e31281713d7fb898acc3c3c291ca1050188a318caa5541ce13fd8bf360c2578248e2c1280fd8da13b3ae0cefbae3b650ccaf05be369240267fcfceac464334c990bf91162c7f2d576442dc70cfb9f8c06f64b14c28a7cfc797043909718842a544e7b7777abf58a88cfe60eb24bd1e602de2caabab89927355909dcf4a55148ecece64db923cc130291a7aa9b6295070b772cd888c1194452b723405f16986b8b2d688b55074a1dc1d0036de48bd2b501569993667f0df1ad5db5b92b07a90bda8db73809e891733ac215c395335325034307f4ec34887397a88adbbccdee3e3ee2aebb1531398ad1d442ef75b922df0cc58292fa30f96ff5d96bd0a76a0001d99be689b1fe4320e96d49a543eea1e80e5d290fe44e05d41dbbef78e025770a4aabe4491ead9687f6f948ffd8003ac60496570588770ceb76e0d887b31d3bef1f701e0314dc95b152dbfa0137c59e5508cc51d7c0c6dfc377bfe0f6beb5aebda155be3e2e97072f211f39767fd8e1a10b106c406e01881f9e2dc2f2c882a1dec89cf90968c0c975b84d79b04aede42f00bf6f94927e7fded543c812be08e0278fa0e7f5ed03674f80eb9dc4f1f2cbeb01f6a7113906bd6b9572e39350055753f23c7c66826b36bbf85ca63d9f2fb5a802abaf2e104a65d94b969f4870fceaf47677581ec403ffe5ccea60c5734792e60c1adb614c1b8bd69e1818ced1650357f7cc2356d462c16bf2ad8e48ccca4c39010c7c3c8364320268f700763b052fffb0f23877fc2d2b7641ce35160e00e27a07138c92e516417a9630b10bff9cafb3b6642df02e45ddcd3fc37b96589f831c08");

    }

    vector<string> txs_blobs;

    for (auto const& tx_hex: txs_hex)
        txs_blobs.push_back(xmreg::hex_to_tx_blob(tx_hex));

    for (auto const& tx_blob: txs_blobs)
    {
        mempool_txs_to_return.emplace_back();
        mempool_txs_to_return.back().tx_blob = tx_blob;
    }


    EXPECT_CALL(*mcore_ptr, get_mempool_txs(_, _))
            .WillRepeatedly(DoAll(
                          SetArgReferee<0>(mempool_txs_to_return),
                          Return(true)));

    block mock_blk; // just an empty block

    EXPECT_CALL(*mcore_ptr, get_block_from_height(_, _))
            .WillRepeatedly(DoAll(SetArgReferee<1>(mock_blk),
                            Return(true)));

    EXPECT_CALL(*mcore_ptr, get_transactions(_, _, _))
            .WillRepeatedly(Return(true));

    ASSERT_TRUE(bcs->read_mempool());


    string tx_hash_with_payment;

    EXPECT_TRUE(bcs->search_if_payment_made(expected_payment_id_str,
                                            desired_amount,
                                            tx_hash_with_payment));

    EXPECT_EQ(tx_hash_with_payment, expected_tx_hash_str);

    // desired amount is more than what tx has, so
    // it should be false
    EXPECT_FALSE(bcs->search_if_payment_made(expected_payment_id_str,
                                            desired_amount*2,
                                            tx_hash_with_payment));

    EXPECT_CALL(*mcore_ptr, get_block_from_height(_, _))
            .WillRepeatedly(DoAll(SetArgReferee<1>(mock_blk),
                            Return(false)));

    EXPECT_FALSE(bcs->search_if_payment_made(expected_payment_id_str,
                                            desired_amount,
                                            tx_hash_with_payment));

    EXPECT_CALL(*mcore_ptr, get_block_from_height(_, _))
            .WillRepeatedly(DoAll(SetArgReferee<1>(mock_blk),
                            Return(true)));

    EXPECT_CALL(*mcore_ptr, get_transactions(_, _, _))
            .WillRepeatedly(Return(false));

    EXPECT_FALSE(bcs->search_if_payment_made(expected_payment_id_str,
                                            desired_amount,
                                            tx_hash_with_payment));
}

TEST_P(BCSTATUS_TEST, GetOutputKey)
{

    output_data_t output_to_return {
                                crypto::rand<crypto::public_key>(),
                                1000, 2222,
                                crypto::rand<rct::key>()};

    EXPECT_CALL(*mcore_ptr, get_output_key(_, _))
            .WillOnce(Return(output_to_return));


    output_data_t result = bcs->get_output_key(333 /* any */,
                                               444 /* any */);

    EXPECT_EQ(result.pubkey, output_to_return.pubkey);
}

ACTION(MockSearchWhile)
{
    cout << "\nMocking while search in txsearch class\n" << endl;
}

TEST_P(BCSTATUS_TEST, StartTxSearchThread)
{
    xmreg::XmrAccount acc; // empty, mock account

    auto tx_search = std::make_unique<MockTxSearch>();

    EXPECT_CALL(*tx_search, operator_fcall()) // mock operator()
            .WillOnce(MockSearchWhile());

    EXPECT_TRUE(bcs->start_tx_search_thread(acc, std::move(tx_search)));

    auto tx_search2 = std::make_unique<MockTxSearch>();

    // trying launching the same thread for already running account
    // should also return true as this is fine
    EXPECT_TRUE(bcs->start_tx_search_thread(acc, std::move(tx_search2)));

    // wait a bit, just to give time to tx_search mock thread to finish
    // its mocking action
    std::this_thread::sleep_for(1s);
}

ACTION(MockSearchWhile2)
{
    cout << "\nMocking while search in txsearch class2\n" << endl;
    std::this_thread::sleep_for(1s);
}

TEST_P(BCSTATUS_TEST, PingSearchThread)
{
    xmreg::XmrAccount acc; // empty, mock account

    acc.address = "whatever mock address";

    auto tx_search = std::make_unique<MockTxSearch>();

    EXPECT_CALL(*tx_search, operator_fcall()) // mock operator()
            .WillOnce(MockSearchWhile2());

    EXPECT_CALL(*tx_search, ping()).WillOnce(Return());

    EXPECT_CALL(*tx_search, still_searching())
            .WillRepeatedly(Return(false));

    ASSERT_TRUE(bcs->start_tx_search_thread(acc, std::move(tx_search)));

    EXPECT_TRUE(bcs->ping_search_thread(acc.address));         

    while(bcs->search_thread_exist(acc.address))
    {
        cout << "\nsearch thread still exists\n";
        std::this_thread::sleep_for(1s);
        bcs->clean_search_thread_map();
    }

    // once we removed the search thread as it finshed,
    // we should be getting false now

    EXPECT_FALSE(bcs->ping_search_thread(acc.address));
}



TEST_P(BCSTATUS_TEST, GetSearchedBlkOutputsAndAddrViewkey)
{
    xmreg::XmrAccount acc; // empty, mock account

    acc.address = "whatever mock address";

    auto tx_search = std::make_unique<MockTxSearch>();

    EXPECT_CALL(*tx_search, operator_fcall()) // mock operator()
            .WillOnce(MockSearchWhile2());

    EXPECT_CALL(*tx_search, get_searched_blk_no())
            .WillOnce(Return(123));

    xmreg::TxSearch::known_outputs_t outputs_to_return;

    outputs_to_return.insert({crypto::rand<crypto::public_key>(), 33});
    outputs_to_return.insert({crypto::rand<crypto::public_key>(), 44});
    outputs_to_return.insert({crypto::rand<crypto::public_key>(), 55});

    EXPECT_CALL(*tx_search, get_known_outputs_keys())
            .WillOnce(Return(outputs_to_return));

    xmreg::TxSearch::addr_view_t mock_address = std::make_pair(
                bcs->get_bc_setup().import_payment_address,
                bcs->get_bc_setup().import_payment_viewkey);

    EXPECT_CALL(*tx_search, get_xmr_address_viewkey())
            .WillRepeatedly(Return(mock_address));

    EXPECT_CALL(*tx_search, still_searching())
            .WillRepeatedly(Return(false));

    ASSERT_TRUE(bcs->start_tx_search_thread(acc, std::move(tx_search)));

    uint64_t searched_blk_no {0};

    EXPECT_TRUE(bcs->get_searched_blk_no(acc.address, searched_blk_no));

    xmreg::TxSearch::known_outputs_t outputs_returned;

    EXPECT_TRUE(bcs->get_known_outputs_keys(acc.address, outputs_returned));

    EXPECT_EQ(outputs_returned.size(), outputs_to_return.size());

    address_parse_info address_returned;
    crypto::secret_key viewkey_returned;

    EXPECT_TRUE(bcs->get_xmr_address_viewkey(acc.address,
                                             address_returned,
                                             viewkey_returned));

    //can compare addresses only as string
    string address_returned_str
            = get_account_address_as_str(GetParam(), false,
                                         address_returned.address);

    string mock_address_str
            = get_account_address_as_str(GetParam(), false,
                                         mock_address.first.address);

    EXPECT_EQ(address_returned_str, mock_address_str);
    EXPECT_EQ(viewkey_returned, mock_address.second);

    while(bcs->search_thread_exist(acc.address))
    {
        cout << "\nsearch thread still exists\n";
        std::this_thread::sleep_for(1s);
        bcs->clean_search_thread_map();
    }

    // once we removed the search thread as it finshed,
    // we should be getting false now

    EXPECT_FALSE(bcs->get_searched_blk_no(acc.address, searched_blk_no));
    EXPECT_FALSE(bcs->get_known_outputs_keys(acc.address, outputs_returned));
    EXPECT_FALSE(bcs->get_xmr_address_viewkey(acc.address,
                                             address_returned,
                                             viewkey_returned));

}


INSTANTIATE_TEST_CASE_P(
        DifferentMoneroNetworks, BCSTATUS_TEST,
        ::testing::Values(
                network_type::MAINNET,
                network_type::TESTNET,
                network_type::STAGENET));

}
