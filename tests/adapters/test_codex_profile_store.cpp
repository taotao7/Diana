#include "adapters/codex_profile_store.h"
#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>

using namespace diana;

class CodexProfileStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_profile_name_ = "diana-unit-test-codex-profile-" +
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    void TearDown() override {
        if (store_.get_profile(test_profile_name_).has_value()) {
            store_.delete_profile(test_profile_name_);
        }
        if (store_.get_profile(renamed_profile_name_).has_value()) {
            store_.delete_profile(renamed_profile_name_);
        }
    }
    
    CodexProfileStore store_;
    std::string test_profile_name_;
    std::string renamed_profile_name_ = "diana-unit-test-codex-renamed";
};

TEST_F(CodexProfileStoreTest, AddAndGetProfile) {
    CodexProfile profile;
    profile.name = test_profile_name_;
    profile.description = "Test description";
    profile.config.model = "gpt-5-codex";
    profile.config.model_provider = "openai";
    
    EXPECT_TRUE(store_.add_profile(profile));
    
    auto retrieved = store_.get_profile(test_profile_name_);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, test_profile_name_);
    EXPECT_EQ(retrieved->description, "Test description");
    EXPECT_EQ(retrieved->config.model, "gpt-5-codex");
}

TEST_F(CodexProfileStoreTest, AddDuplicateProfile) {
    CodexProfile profile;
    profile.name = test_profile_name_;
    
    EXPECT_TRUE(store_.add_profile(profile));
    EXPECT_FALSE(store_.add_profile(profile));
}

TEST_F(CodexProfileStoreTest, UpdateProfile) {
    CodexProfile profile;
    profile.name = test_profile_name_;
    profile.config.model = "original-model";
    EXPECT_TRUE(store_.add_profile(profile));
    
    profile.config.model = "updated-model";
    profile.description = "Updated description";
    EXPECT_TRUE(store_.update_profile(test_profile_name_, profile));
    
    auto updated = store_.get_profile(test_profile_name_);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->config.model, "updated-model");
    EXPECT_EQ(updated->description, "Updated description");
}

TEST_F(CodexProfileStoreTest, DeleteProfile) {
    CodexProfile profile;
    profile.name = test_profile_name_;
    EXPECT_TRUE(store_.add_profile(profile));
    
    EXPECT_TRUE(store_.get_profile(test_profile_name_).has_value());
    EXPECT_TRUE(store_.delete_profile(test_profile_name_));
    EXPECT_FALSE(store_.get_profile(test_profile_name_).has_value());
}

TEST_F(CodexProfileStoreTest, RenameProfile) {
    CodexProfile profile;
    profile.name = test_profile_name_;
    profile.config.model = "test-model";
    EXPECT_TRUE(store_.add_profile(profile));
    
    EXPECT_TRUE(store_.rename_profile(test_profile_name_, renamed_profile_name_));
    
    EXPECT_FALSE(store_.get_profile(test_profile_name_).has_value());
    auto renamed = store_.get_profile(renamed_profile_name_);
    ASSERT_TRUE(renamed.has_value());
    EXPECT_EQ(renamed->config.model, "test-model");
}

TEST_F(CodexProfileStoreTest, RenameToSameName) {
    CodexProfile profile;
    profile.name = test_profile_name_;
    EXPECT_TRUE(store_.add_profile(profile));
    
    EXPECT_TRUE(store_.rename_profile(test_profile_name_, test_profile_name_));
    EXPECT_TRUE(store_.get_profile(test_profile_name_).has_value());
}
