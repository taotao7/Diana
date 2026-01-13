# Homebrew Tap for Diana

This directory contains Homebrew formulas for installing Diana.

## Setup Homebrew Tap

To allow users to install Diana via Homebrew, you need to create a separate repository named `homebrew-diana`.

### Step 1: Create the Tap Repository

1. Create a new GitHub repository: `taotao7/homebrew-diana`

2. Clone it and copy the formula files:

```bash
git clone git@github.com:taotao7/homebrew-diana.git
cd homebrew-diana

# Copy formulas from this directory
cp -r /path/to/Diana/homebrew/* .

# Commit and push
git add .
git commit -m "Initial formula"
git push
```

### Step 2: Update SHA256 Hashes

Before publishing, you need to calculate the SHA256 hashes for the release artifacts:

#### For Formula (source tarball):
```bash
# After creating a GitHub release tag
curl -sL https://github.com/taotao7/Diana/archive/refs/tags/v0.1.1.tar.gz | shasum -a 256
```

Update `Formula/diana.rb` with the calculated hash.

#### For Cask (DMG files):
```bash
# Calculate hash for ARM64 DMG
shasum -a 256 Diana-0.1.1-arm64.dmg

# Calculate hash for x86_64 DMG  
shasum -a 256 Diana-0.1.1-x86_64.dmg
```

Update `Casks/diana.rb` with the calculated hashes.

### Step 3: Create GitHub Release

1. Tag the release:
```bash
git tag -a v0.1.1 -m "Release v0.1.1"
git push origin v0.1.1
```

2. Build and upload DMG files:
```bash
# Build for current architecture
VERSION=0.1.1 ./scripts/package-dmg.sh

# Upload the DMG to GitHub Releases
```

3. If you need to support both ARM64 and x86_64, build on both architectures.

## User Installation

Once the tap is set up, users can install Diana with:

### Option 1: Cask (Recommended - GUI app)
```bash
brew tap taotao7/diana
brew install --cask diana
```

### Option 2: Formula (CLI binary from source)
```bash
brew tap taotao7/diana
brew install diana
```

### Option 3: Direct install without tapping
```bash
brew install --cask https://raw.githubusercontent.com/taotao7/Diana/v0.1.1/homebrew/Casks/diana.rb
brew install https://raw.githubusercontent.com/taotao7/Diana/v0.1.1/homebrew/Formula/diana.rb
```

## Directory Structure

```
homebrew-diana/           # This becomes the tap repository
├── Formula/
│   └── diana.rb          # Source build formula
├── Casks/
│   └── diana.rb          # DMG-based cask
└── README.md
```

## Updating Versions

When releasing a new version:

1. Update version in `CMakeLists.txt`
2. Create a new git tag
3. Build and upload DMG to GitHub Releases
4. Update `homebrew-diana` repository:
   - Update version numbers in formulas
   - Update SHA256 hashes
   - Commit and push

## CI/CD Integration

Consider adding a GitHub Actions workflow to automate:
- Building DMG on release
- Calculating SHA256 hashes
- Updating the homebrew-diana repository

See `.github/workflows/release.yml` for an example workflow.
