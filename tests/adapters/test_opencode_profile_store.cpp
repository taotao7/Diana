#include <gtest/gtest.h>
#include "adapters/opencode_profile_store.h"
#include <filesystem>
#include <fstream>

using namespace diana;

TEST(OpenCodeProfileStoreTest, StorePath) {
    OpenCodeProfileStore store;
    EXPECT_FALSE(store.store_path().empty());
    EXPECT_NE(store.store_path().find("opencode_profiles.json"), std::string::npos);
}

TEST(OpenCodeProfileStoreTest, ConfigPath) {
    OpenCodeProfileStore store;
    EXPECT_FALSE(store.config_path().empty());
    EXPECT_NE(store.config_path().find("opencode.json"), std::string::npos);
}

TEST(OpenCodeProfileStoreTest, LoadEmptyStoreSucceeds) {
    OpenCodeProfileStore store;
    EXPECT_TRUE(store.load());
}

TEST(OpenCodeProfileStoreTest, GetNonexistentProfileReturnsNullopt) {
    OpenCodeProfileStore store;
    store.load();
    
    auto result = store.get_profile("definitely-nonexistent-profile-xyz123");
    EXPECT_FALSE(result.has_value());
}

TEST(OpenCodeProfileStoreTest, SetActiveNonexistentProfileFails) {
    OpenCodeProfileStore store;
    store.load();
    
    EXPECT_FALSE(store.set_active_profile("definitely-nonexistent-profile-xyz123"));
}

TEST(OpenCodeProfileStoreTest, DeleteNonexistentProfileFails) {
    OpenCodeProfileStore store;
    store.load();
    
    EXPECT_FALSE(store.delete_profile("definitely-nonexistent-profile-xyz123"));
}

TEST(OpenCodeProfileStoreTest, RenameNonexistentProfileFails) {
    OpenCodeProfileStore store;
    store.load();
    
    EXPECT_FALSE(store.rename_profile("definitely-nonexistent-xyz123", "new-name"));
}

TEST(OpenCodeProfileStoreTest, UpdateNonexistentProfileFails) {
    OpenCodeProfileStore store;
    store.load();
    
    OpenCodeProfile profile;
    profile.name = "definitely-nonexistent-xyz123";
    
    EXPECT_FALSE(store.update_profile("definitely-nonexistent-xyz123", profile));
}

class OpenCodeProfileStoreIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_profile_name_ = "diana-unit-test-profile-" + 
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        
        store_.load();
        
        if (store_.get_profile(test_profile_name_).has_value()) {
            store_.delete_profile(test_profile_name_);
        }
    }
    
    void TearDown() override {
        if (store_.get_profile(test_profile_name_).has_value()) {
            store_.delete_profile(test_profile_name_);
        }
        if (store_.get_profile(renamed_profile_name_).has_value()) {
            store_.delete_profile(renamed_profile_name_);
        }
    }
    
    OpenCodeProfileStore store_;
    std::string test_profile_name_;
    std::string renamed_profile_name_ = "diana-unit-test-renamed";
};

TEST_F(OpenCodeProfileStoreIntegrationTest, AddAndRetrieveProfile) {
    OpenCodeProfile profile;
    profile.name = test_profile_name_;
    profile.description = "Test description";
    profile.config.model = "anthropic/claude-sonnet-4-20250514";
    profile.config.theme = "opencode";
    
    EXPECT_TRUE(store_.add_profile(profile));
    
    auto retrieved = store_.get_profile(test_profile_name_);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, test_profile_name_);
    EXPECT_EQ(retrieved->description, "Test description");
    EXPECT_EQ(retrieved->config.model, "anthropic/claude-sonnet-4-20250514");
    EXPECT_GT(retrieved->created_at, 0);
}

TEST_F(OpenCodeProfileStoreIntegrationTest, AddDuplicateProfileFails) {
    OpenCodeProfile profile;
    profile.name = test_profile_name_;
    
    EXPECT_TRUE(store_.add_profile(profile));
    EXPECT_FALSE(store_.add_profile(profile));
}

TEST_F(OpenCodeProfileStoreIntegrationTest, UpdateProfile) {
    OpenCodeProfile profile;
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

TEST_F(OpenCodeProfileStoreIntegrationTest, DeleteProfile) {
    OpenCodeProfile profile;
    profile.name = test_profile_name_;
    EXPECT_TRUE(store_.add_profile(profile));
    
    EXPECT_TRUE(store_.get_profile(test_profile_name_).has_value());
    EXPECT_TRUE(store_.delete_profile(test_profile_name_));
    EXPECT_FALSE(store_.get_profile(test_profile_name_).has_value());
}

TEST_F(OpenCodeProfileStoreIntegrationTest, RenameProfile) {
    OpenCodeProfile profile;
    profile.name = test_profile_name_;
    profile.config.model = "test-model";
    EXPECT_TRUE(store_.add_profile(profile));
    
    EXPECT_TRUE(store_.rename_profile(test_profile_name_, renamed_profile_name_));
    
    EXPECT_FALSE(store_.get_profile(test_profile_name_).has_value());
    auto renamed = store_.get_profile(renamed_profile_name_);
    ASSERT_TRUE(renamed.has_value());
    EXPECT_EQ(renamed->config.model, "test-model");
}

TEST_F(OpenCodeProfileStoreIntegrationTest, RenameSameNameSucceeds) {
    OpenCodeProfile profile;
    profile.name = test_profile_name_;
    EXPECT_TRUE(store_.add_profile(profile));
    
    EXPECT_TRUE(store_.rename_profile(test_profile_name_, test_profile_name_));
    EXPECT_TRUE(store_.get_profile(test_profile_name_).has_value());
}
