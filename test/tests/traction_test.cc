#include "utils/async_if_test_helper.hxx"

#include "nmranet/TractionTrain.hxx"
#include "nmranet/TractionDefs.hxx"

namespace NMRAnet
{

TEST(Fp16Test, NanToSpeedIsNan)
{
    SpeedType nan_speed = nan_to_speed();
    EXPECT_TRUE(isnan(nan_speed.speed()));
}

TEST(Fp16Test, FFFFToSpeedIsNan)
{
    uint16_t nan_fp16 = 0xffffU;
    SpeedType nan_speed = fp16_to_speed(&nan_fp16);
    EXPECT_TRUE(isnan(nan_speed.speed()));

    uint32_t fp_nan = 0;
    memcpy(&fp_nan, &nan_speed, 4);
    fprintf(stderr, "%08" PRIx32 "\n", fp_nan);
}

TEST(Fp16Test, NanToSpeedTo16Correct)
{
    SpeedType nan_speed = nan_to_speed();
    uint32_t nan_speed_raw;
    memcpy(&nan_speed_raw, &nan_speed, 4);
    EXPECT_EQ(0xffff0000U, nan_speed_raw);

    uint16_t nan_in_fp16;
    speed_to_fp16(nan_speed, &nan_in_fp16);
    EXPECT_EQ(0xffffU, nan_in_fp16);
}

TEST(Fp16Test, FFFFToSpeedTofp16IsFFFF)
{
    uint16_t original = 0xffffU;
    SpeedType sp = fp16_to_speed(&original);
    uint16_t reconv_fp16;
    speed_to_fp16(sp, &reconv_fp16);
    EXPECT_EQ(0xffffU, reconv_fp16);
}

TEST(Fp16Test, UnalignedSave)
{
    SpeedType s = 37.5;
    uint32_t v = 0;
    uint8_t *b = reinterpret_cast<uint8_t *>(&v);
    speed_to_fp16(s, b + 1);
    EXPECT_EQ(0U, v & 0xFF0000FFU);
    SpeedType ss = fp16_to_speed(b + 1);
    EXPECT_EQ(37.5, ss.speed());
}

// The wire format shall be big endian. This is binary 1.001011 * 2^-5
uint8_t k37_5[] = {0x50, 0xB0};

TEST(Fp16Test, ThirtySevenAndAHalf)
{
    SpeedType s = fp16_to_speed(k37_5);
    EXPECT_EQ(37.5, s.speed());
}

class MockTrain : public TrainImpl
{
public:
    MOCK_METHOD1(set_speed, void(SpeedType speed));
    MOCK_METHOD0(get_speed, SpeedType());
    MOCK_METHOD0(get_commanded_speed, SpeedType());
    MOCK_METHOD0(get_actual_speed, SpeedType());
    MOCK_METHOD0(set_emergencystop, void());
    MOCK_METHOD2(set_fn, void(uint32_t address, uint16_t value));
    MOCK_METHOD1(get_fn, uint16_t(uint32_t address));
    MOCK_METHOD0(legacy_address, uint32_t());
};

class TractionTest : public AsyncNodeTest
{
protected:
    TractionTest()
        : trainService_(ifCan_.get())
    {
    }

    TrainService trainService_;
    StrictMock<MockTrain> m1_, m2_;
};

class TractionSingleMockTest : public TractionTest
{
protected:
    TractionSingleMockTest()
    {
        create_allocated_alias();
        expect_next_alias_allocation();
        EXPECT_CALL(m1_, legacy_address()).Times(AtLeast(0)).WillRepeatedly(
            Return(0x00003456U));
        // alias reservation
        expect_packet(":X1070133AN060100003456;");
        // initialized
        expect_packet(":X1910033AN060100003456;");
        trainNode_.reset(new TrainNode(&trainService_, &m1_));
        wait();
    }
    ~TractionSingleMockTest()
    {
        wait();
    }

    std::unique_ptr<TrainNode> trainNode_;
};

TEST_F(TractionSingleMockTest, SetSpeed)
{
    LOG(INFO, "node %p", trainNode_.get());
    EXPECT_CALL(m1_, set_speed(Velocity(37.5)));
    send_packet(":X195EA551N033A0050B0;");
}

TEST_F(TractionSingleMockTest, GetSpeed)
{
    EXPECT_CALL(m1_, get_speed()).WillOnce(Return(37.5));
    EXPECT_CALL(m1_, get_commanded_speed()).WillOnce(Return(nan_to_speed()));
    EXPECT_CALL(m1_, get_actual_speed()).WillOnce(Return(nan_to_speed()));
    expect_packet(":X195E833AN15511050B000FFFF;");
    expect_packet(":X195E833AN2551FFFF;");
    send_packet(":X195EA551N033A10;");
}

TEST_F(TractionSingleMockTest, GetSpeedTestWithCommandedSpeed)
{
    EXPECT_CALL(m1_, get_speed()).WillOnce(Return(37.5));
    EXPECT_CALL(m1_, get_commanded_speed()).WillOnce(Return(37.0));
    EXPECT_CALL(m1_, get_actual_speed()).WillOnce(Return(nan_to_speed()));
    expect_packet(":X195E833AN15511050B00050A0;");
    expect_packet(":X195E833AN2551FFFF;");
    send_packet(":X195EA551N033A10;");
}

TEST_F(TractionSingleMockTest, GetSpeedTestWithActualSpeed)
{
    EXPECT_CALL(m1_, get_speed()).WillOnce(Return(37.5));
    EXPECT_CALL(m1_, get_commanded_speed()).WillOnce(Return(37.0));
    EXPECT_CALL(m1_, get_actual_speed()).WillOnce(Return(38.0));
    expect_packet(":X195E833AN15511050B00050A0;");
    expect_packet(":X195E833AN255150C0;");
    send_packet(":X195EA551N033A10;");
}

TEST_F(TractionSingleMockTest, SetFn)
{
    EXPECT_CALL(m1_, set_fn(0x112233, 0x4384));
    send_packet(":X195EA551N033A011122334384;");
}

TEST_F(TractionSingleMockTest, GetFn)
{
    EXPECT_CALL(m1_, get_fn(0x332244)).WillOnce(Return(0x6622));
    send_packet_and_expect_response(":X195EA551N033A11332244;",
                                    ":X195E833AN0551113322446622;");
}

} // namespace NMRAnet