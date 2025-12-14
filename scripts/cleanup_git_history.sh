#!/bin/bash

# Script để xóa sensitive URLs/IPs khỏi git history
# Sử dụng git-filter-repo (khuyến nghị) hoặc git filter-branch

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BACKUP_DIR="${REPO_DIR}_backup_$(date +%Y%m%d_%H%M%S)"
METHOD="${1:-filter-repo}"  # filter-repo, bfg, hoặc filter-branch

echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Git History Cleanup Script${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""

# Check if we're in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo -e "${RED}Error: Not a git repository${NC}"
    exit 1
fi

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo -e "${RED}Error: You have uncommitted changes. Please commit or stash them first.${NC}"
    exit 1
fi

# Confirm action
echo -e "${YELLOW}⚠️  WARNING: This will rewrite git history!${NC}"
echo -e "${YELLOW}⚠️  All team members will need to re-clone the repository${NC}"
echo ""
read -p "Are you sure you want to continue? (yes/no): " confirm

if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 0
fi

# Create backup
echo -e "${GREEN}Creating backup...${NC}"
cd "$REPO_DIR/.."
git clone --mirror "$(basename "$REPO_DIR")" "$(basename "$BACKUP_DIR").git"
echo -e "${GREEN}✓ Backup created at: $(basename "$BACKUP_DIR").git${NC}"
echo ""

cd "$REPO_DIR"

# Create replacement file
REPLACEMENT_FILE=$(mktemp)
cat > "$REPLACEMENT_FILE" << 'EOF'
localhost==>localhost
localhost==>localhost
localhost==>localhost
localhost==>localhost
localhost==>localhost
localhost==>localhost
EOF

echo -e "${GREEN}Replacement patterns:${NC}"
cat "$REPLACEMENT_FILE"
echo ""

# Method 1: git-filter-repo (Recommended)
if [ "$METHOD" = "filter-repo" ]; then
    echo -e "${GREEN}Using git-filter-repo method...${NC}"
    
    # Check if git-filter-repo is installed
    if ! command -v git-filter-repo &> /dev/null; then
        echo -e "${RED}Error: git-filter-repo is not installed${NC}"
        echo "Install it with: sudo apt-get install git-filter-repo"
        echo "Or: pip install git-filter-repo"
        exit 1
    fi
    
    # Run git-filter-repo
    git filter-repo --replace-text "$REPLACEMENT_FILE" --force
    
    echo -e "${GREEN}✓ History cleaned using git-filter-repo${NC}"

# Method 2: BFG Repo-Cleaner
elif [ "$METHOD" = "bfg" ]; then
    echo -e "${GREEN}Using BFG Repo-Cleaner method...${NC}"
    
    BFG_JAR="${REPO_DIR}/bfg.jar"
    if [ ! -f "$BFG_JAR" ]; then
        echo -e "${YELLOW}Downloading BFG Repo-Cleaner...${NC}"
        wget -q https://repo1.maven.org/maven2/com/madgag/bfg/1.14.0/bfg-1.14.0.jar -O "$BFG_JAR"
    fi
    
    # Create sensitive strings file for BFG
    SENSITIVE_FILE=$(mktemp)
    cat > "$SENSITIVE_FILE" << 'EOF'
localhost
localhost
localhost
localhost
localhost
localhost
EOF
    
    # Clone bare repo for BFG
    BARE_REPO="${REPO_DIR}_bare"
    rm -rf "$BARE_REPO"
    git clone --mirror "$REPO_DIR" "$BARE_REPO"
    
    cd "$BARE_REPO"
    java -jar "$BFG_JAR" --replace-text "$REPLACEMENT_FILE"
    
    git reflog expire --expire=now --all
    git gc --prune=now --aggressive
    
    echo -e "${GREEN}✓ History cleaned using BFG${NC}"
    echo -e "${YELLOW}Note: You need to copy changes back to original repo${NC}"

# Method 3: git filter-branch (Legacy)
elif [ "$METHOD" = "filter-branch" ]; then
    echo -e "${YELLOW}Using git filter-branch (legacy method - slower)...${NC}"
    
    # Create a script to replace strings
    REPLACE_SCRIPT=$(mktemp)
    cat > "$REPLACE_SCRIPT" << 'SCRIPT'
#!/bin/bash
sed -i 's/anhoidong\.datacenter\.cvedix\.com/localhost/g' "$@"
sed -i 's/192\.168\.1\.100/localhost/g' "$@"
sed -i 's/192\.168\.1\.106/localhost/g' "$@"
sed -i 's/192\.168\.1\.200/localhost/g' "$@"
sed -i 's/103\.147\.186\.175/localhost/g' "$@"
sed -i 's/mqtt\.goads\.com\.vn/localhost/g' "$@"
SCRIPT
    chmod +x "$REPLACE_SCRIPT"
    
    git filter-branch --force --tree-filter "$REPLACE_SCRIPT" --prune-empty --tag-name-filter cat -- --all
    
    # Clean up
    git for-each-ref --format="%(refname)" refs/original/ | xargs -n 1 git update-ref -d
    git reflog expire --expire=now --all
    git gc --prune=now --aggressive
    
    echo -e "${GREEN}✓ History cleaned using git filter-branch${NC}"
else
    echo -e "${RED}Error: Unknown method: $METHOD${NC}"
    echo "Available methods: filter-repo, bfg, filter-branch"
    exit 1
fi

# Cleanup temp files
rm -f "$REPLACEMENT_FILE"

# Verify cleanup
echo ""
echo -e "${GREEN}Verifying cleanup...${NC}"
if git log --all --full-history -S "localhost" | head -1 | grep -q .; then
    echo -e "${RED}⚠️  Warning: Still found sensitive data in history!${NC}"
    echo "You may need to run this script again or use a different method."
else
    echo -e "${GREEN}✓ Verification passed - no sensitive data found${NC}"
fi

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Cleanup completed!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Review changes: git log --all"
echo "2. Test the repository"
echo "3. Force push (if ready): git push --force --all"
echo "4. Notify team members to re-clone"
echo ""
echo -e "${YELLOW}Backup location: ${BACKUP_DIR}${NC}"

