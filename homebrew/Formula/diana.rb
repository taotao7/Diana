class Diana < Formula
  desc "Mission control for AI agents - unified config management and token monitoring"
  homepage "https://github.com/taotao7/Diana"
  url "https://github.com/taotao7/Diana/archive/refs/tags/v0.1.1.tar.gz"
  sha256 "a7f7448b7edb7e5bc689caba617de26da19cebacf90897b08ce002e9c2c841d7"
  license "MIT"
  head "https://github.com/taotao7/Diana.git", branch: "main"

  depends_on "cmake" => :build
  depends_on xcode: :build
  depends_on :macos

  def install
    system "cmake", "-S", ".", "-B", "build",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DDIANA_BUILD_TESTS=OFF",
                    *std_cmake_args
    system "cmake", "--build", "build", "--parallel"

    bin.install "build/diana"

    (share/"diana/fonts").install Dir["resources/fonts/*"]
  end

  def caveats
    <<~EOS
      Diana has been installed.

      To run Diana:
        diana

      Note: Diana is a GUI application. For best experience, consider using
      the Cask version which installs Diana.app:
        brew install --cask diana
    EOS
  end

  test do
    assert_predicate bin/"diana", :exist?
    assert_predicate bin/"diana", :executable?
  end
end
