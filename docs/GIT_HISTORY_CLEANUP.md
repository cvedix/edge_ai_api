# Hướng Dẫn Xóa Sensitive Data Khỏi Git History

## ⚠️ CẢNH BÁO QUAN TRỌNG

**Trước khi thực hiện:**
1. ✅ **BACKUP repository** - Tạo backup đầy đủ trước khi chạy
2. ✅ **Thông báo team** - Tất cả thành viên cần được thông báo
3. ✅ **Force push sẽ rewrite history** - Tất cả người dùng cần re-clone hoặc reset local branches
4. ✅ **Nếu repo đã public** - Cần thay đổi credentials/URLs thực tế vì đã có thể bị lộ

---

## 🔍 Kiểm Tra Sensitive Data trong History

### 1. Tìm các URL thực tế trong history:

```bash
# Tìm tất cả commits chứa URL thực tế
git log --all --full-history -S "localhost" --pretty=format:"%H %s %ad" --date=short

# Tìm trong tất cả files
git log --all --full-history --source -- "*" | grep -i "anhoidong\|192.168\|server.com"

# Xem file nào chứa sensitive data
git log --all --full-history --diff-filter=D --summary | grep -i "anhoidong\|192.168"
```

### 2. Kiểm tra số lượng commits bị ảnh hưởng:

```bash
# Đếm số commits chứa URL thực tế
git log --all --full-history -S "localhost" --oneline | wc -l
```

---

## 🛠️ Giải Pháp: Xóa Sensitive Data

### Phương Pháp 1: Sử dụng `git filter-repo` (Khuyến nghị) ⭐

**Ưu điểm:** Nhanh, an toàn, dễ sử dụng

#### Bước 1: Cài đặt git-filter-repo

```bash
# Ubuntu/Debian
sudo apt-get install git-filter-repo

# Hoặc cài qua pip
pip install git-filter-repo

# Hoặc cài từ source
git clone https://github.com/newren/git-filter-repo.git
cd git-filter-repo
sudo make install
```

#### Bước 2: Backup repository

```bash
cd /home/cvedix/project/edge_ai_api
cd ..
git clone --mirror edge_ai_api edge_ai_api_backup.git
```

#### Bước 3: Tạo file chứa các patterns cần xóa

```bash
# Tạo file .git/filter-repo-expressions.txt
cat > /tmp/filter-repo-expressions.txt << 'EOF'
anhoidong\.datacenter\.cvedix\.com
192\.168\.1\.(100|106|200)
103\.147\.186\.175
mqtt\.goads\.com\.vn
EOF
```

#### Bước 4: Chạy git-filter-repo để thay thế

```bash
cd /home/cvedix/project/edge_ai_api

# Thay thế tất cả occurrences
git filter-repo --replace-text <(cat << 'EOF'
localhost==>localhost
localhost==>localhost
localhost==>localhost
localhost==>localhost
localhost==>localhost
localhost==>localhost
EOF
)
```

**Lưu ý:** `git filter-repo` sẽ tự động:
- Rewrite toàn bộ history
- Update refs
- Remove backup refs

---

### Phương Pháp 2: Sử dụng BFG Repo-Cleaner

**Ưu điểm:** Dễ sử dụng, có GUI

#### Bước 1: Download BFG

```bash
# Download từ https://rtyley.github.io/bfg-repo-cleaner/
wget https://repo1.maven.org/maven2/com/madgag/bfg/1.14.0/bfg-1.14.0.jar
```

#### Bước 2: Tạo file chứa sensitive strings

```bash
cat > /tmp/sensitive-urls.txt << 'EOF'
localhost
localhost
localhost
localhost
localhost
localhost
EOF
```

#### Bước 3: Clone bare repository

```bash
cd /home/cvedix/project
git clone --mirror edge_ai_api edge_ai_api_clean.git
cd edge_ai_api_clean.git
```

#### Bước 4: Chạy BFG

```bash
# Thay thế sensitive strings
java -jar bfg-1.14.0.jar --replace-text /tmp/sensitive-urls.txt

# Clean up
git reflog expire --expire=now --all
git gc --prune=now --aggressive
```

#### Bước 5: Push lại

```bash
git push --force --all
git push --force --tags
```

---

### Phương Pháp 3: Sử dụng git filter-branch (Cũ, chậm)

**⚠️ Không khuyến nghị** nhưng vẫn có thể dùng nếu không có git-filter-repo

```bash
cd /home/cvedix/project/edge_ai_api

# Backup
git branch backup-before-cleanup

# Thay thế trong toàn bộ history
git filter-branch --force --index-filter \
  'git ls-files -s | sed "s/\t\"*/&/" | \
  GIT_INDEX_FILE=$GIT_INDEX_FILE.new \
  git update-index --index-info && \
  mv "$GIT_INDEX_FILE.new" "$GIT_INDEX_FILE"' \
  --prune-empty --tag-name-filter cat -- --all

# Clean up
git for-each-ref --format="%(refname)" refs/original/ | xargs -n 1 git update-ref -d
git reflog expire --expire=now --all
git gc --prune=now --aggressive
```

---

## 📋 Checklist Sau Khi Cleanup

### 1. Verify cleanup thành công

```bash
# Kiểm tra không còn URL thực tế
git log --all --full-history -S "localhost"
# Kết quả: không có gì

# Kiểm tra trong tất cả files
git grep "localhost"
# Kết quả: không có gì
```

### 2. Force push (nếu cần)

```bash
# ⚠️ CẢNH BÁO: Chỉ làm nếu chắc chắn
git push --force --all
git push --force --tags
```

### 3. Thông báo team

**Email template:**

```
Subject: [URGENT] Repository History Rewritten - Action Required

Hi team,

We have cleaned sensitive data (URLs, IPs) from git history for security reasons.

ACTION REQUIRED:
1. Delete your local repository
2. Re-clone from remote:
   git clone <repository-url>
   
OR if you have uncommitted changes:
1. Backup your changes
2. git fetch origin
3. git reset --hard origin/main  # or your branch name

The repository history has been rewritten, so your local copy is incompatible.

Thanks!
```

### 4. Update CI/CD

- Nếu có CI/CD pipelines, có thể cần update
- Checkout lại repository trong pipelines

---

## 🔐 Bảo Mật Bổ Sung

### Nếu Repository Đã Public:

1. **Thay đổi credentials ngay lập tức:**
   - Đổi passwords cho các services
   - Rotate API keys
   - Thay đổi URLs/endpoints nếu có thể

2. **Monitor access logs:**
   - Kiểm tra logs của các services có URL/IP đã bị lộ
   - Tìm suspicious activities

3. **Consider repository migration:**
   - Nếu repo đã public lâu, có thể cần tạo repo mới
   - Hoặc sử dụng private repository

---

## 📝 Script Tự Động

Xem file `scripts/cleanup_git_history.sh` để có script tự động.

---

## ⚠️ Lưu Ý Quan Trọng

1. **Không thể hoàn tác** sau khi force push
2. **Tất cả team members** phải re-clone
3. **CI/CD pipelines** cần được update
4. **Backup** là bắt buộc trước khi chạy
5. **Test trên branch riêng** trước khi apply lên main

---

## 📚 Tài Liệu Tham Khảo

- [git-filter-repo Documentation](https://github.com/newren/git-filter-repo)
- [BFG Repo-Cleaner](https://rtyley.github.io/bfg-repo-cleaner/)
- [GitHub: Removing sensitive data](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository)

