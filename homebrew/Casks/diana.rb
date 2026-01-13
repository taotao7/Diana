cask "diana" do
  version "0.1.1"
  sha256 "PLACEHOLDER_ARM64_SHA256"

  url "https://github.com/taotao7/Diana/releases/download/v#{version}/Diana-#{version}-arm64.dmg"

  name "Diana"
  desc "Mission control for AI agents - unified config management and token monitoring"
  homepage "https://github.com/taotao7/Diana"

  livecheck do
    url :url
    strategy :github_latest
  end

  depends_on macos: ">= :monterey"
  depends_on arch: :arm64

  app "Diana.app"

  zap trash: [
    "~/.config/diana",
    "~/Library/Application Support/Diana",
    "~/Library/Preferences/com.diana.app.plist",
    "~/Library/Saved Application State/com.diana.app.savedState",
  ]
end
