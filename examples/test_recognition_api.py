#!/usr/bin/env python3
"""
Recognition API Test Script

Script này cung cấp các ví dụ để test các chức năng của Recognition API.
Sử dụng: python test_recognition_api.py

Requirements:
    pip install requests pillow
"""

import requests
import base64
import json
import sys
import os
from pathlib import Path

# Base URL của API
BASE_URL = "http://localhost:8080/v1/recognition"

# Colors for terminal output
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def print_success(message):
    print(f"{Colors.GREEN}✓ {message}{Colors.RESET}")

def print_error(message):
    print(f"{Colors.RED}✗ {message}{Colors.RESET}")

def print_info(message):
    print(f"{Colors.BLUE}ℹ {message}{Colors.RESET}")

def print_warning(message):
    print(f"{Colors.YELLOW}⚠ {message}{Colors.RESET}")

def print_section(title):
    print(f"\n{Colors.BOLD}{'='*60}{Colors.RESET}")
    print(f"{Colors.BOLD}{title}{Colors.RESET}")
    print(f"{Colors.BOLD}{'='*60}{Colors.RESET}\n")


# ============================================================================
# 1. ĐĂNG KÝ KHUÔN MẶT (Register Face)
# ============================================================================

def register_face_multipart(subject_name, image_path, det_threshold=0.5):
    """
    Đăng ký khuôn mặt sử dụng multipart/form-data
    
    Args:
        subject_name: Tên subject cần đăng ký
        image_path: Đường dẫn đến file ảnh
        det_threshold: Ngưỡng phát hiện (0.0-1.0)
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/faces"
    params = {
        "subject": subject_name,
        "det_prob_threshold": det_threshold
    }
    
    try:
        with open(image_path, "rb") as f:
            files = {"file": (os.path.basename(image_path), f, "image/jpeg")}
            response = requests.post(url, params=params, files=files, timeout=30)
        
        if response.status_code == 200:
            print_success(f"Đăng ký thành công: {response.json()}")
            return response.json()
        else:
            print_error(f"Đăng ký thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi đăng ký: {str(e)}")
        return None


def register_face_base64(subject_name, image_path):
    """
    Đăng ký khuôn mặt sử dụng base64-encoded JSON
    
    Args:
        subject_name: Tên subject cần đăng ký
        image_path: Đường dẫn đến file ảnh
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/faces"
    params = {"subject": subject_name}
    
    try:
        with open(image_path, "rb") as f:
            image_b64 = base64.b64encode(f.read()).decode("utf-8")
        
        data = {"file": image_b64}
        headers = {"Content-Type": "application/json"}
        response = requests.post(url, params=params, json=data, headers=headers, timeout=30)
        
        if response.status_code == 200:
            print_success(f"Đăng ký thành công (Base64): {response.json()}")
            return response.json()
        else:
            print_error(f"Đăng ký thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi đăng ký: {str(e)}")
        return None


# ============================================================================
# 2. NHẬN DIỆN KHUÔN MẶT (Recognize Face)
# ============================================================================

def recognize_face(image_path, det_threshold=0.5, prediction_count=3, limit=0):
    """
    Nhận diện khuôn mặt trong ảnh
    
    Args:
        image_path: Đường dẫn đến file ảnh
        det_threshold: Ngưỡng phát hiện (0.0-1.0)
        prediction_count: Số lượng predictions trả về cho mỗi khuôn mặt
        limit: Số lượng khuôn mặt tối đa (0 = không giới hạn)
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/recognize"
    params = {
        "det_prob_threshold": det_threshold,
        "prediction_count": prediction_count,
        "limit": limit
    }
    
    try:
        with open(image_path, "rb") as f:
            files = {"file": (os.path.basename(image_path), f, "image/jpeg")}
            response = requests.post(url, params=params, files=files, timeout=30)
        
        if response.status_code == 200:
            result = response.json()
            print_success(f"Nhận diện thành công: Tìm thấy {len(result.get('result', []))} khuôn mặt")
            return result
        else:
            print_error(f"Nhận diện thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi nhận diện: {str(e)}")
        return None


# ============================================================================
# 3. LIỆT KÊ SUBJECTS (List Faces)
# ============================================================================

def list_faces(page=0, size=20, subject_filter=None):
    """
    Liệt kê tất cả subjects với pagination
    
    Args:
        page: Số trang (bắt đầu từ 0)
        size: Số lượng items mỗi trang
        subject_filter: Lọc theo subject name (None = tất cả)
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/faces"
    params = {"page": page, "size": size}
    if subject_filter:
        params["subject"] = subject_filter
    
    try:
        response = requests.get(url, params=params, timeout=10)
        
        if response.status_code == 200:
            result = response.json()
            total = result.get("total_elements", 0)
            print_success(f"Liệt kê thành công: {total} subjects (trang {page})")
            return result
        else:
            print_error(f"Liệt kê thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi liệt kê: {str(e)}")
        return None


# ============================================================================
# 4. XÓA SUBJECT (Delete Face)
# ============================================================================

def delete_face(image_id_or_subject):
    """
    Xóa subject theo image_id hoặc subject name
    
    Args:
        image_id_or_subject: Image ID hoặc subject name
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/faces/{image_id_or_subject}"
    
    try:
        response = requests.delete(url, timeout=10)
        
        if response.status_code == 200:
            result = response.json()
            print_success(f"Xóa thành công: {json.dumps(result, indent=2)}")
            return result
        elif response.status_code == 404:
            print_warning(f"Không tìm thấy: {image_id_or_subject}")
            return None
        else:
            print_error(f"Xóa thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi xóa: {str(e)}")
        return None


# ============================================================================
# 5. XÓA NHIỀU SUBJECTS (Delete Multiple Faces)
# ============================================================================

def delete_multiple_faces(image_ids):
    """
    Xóa nhiều subjects cùng lúc
    
    Args:
        image_ids: List các image IDs cần xóa
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/faces/delete"
    
    try:
        response = requests.post(url, json=image_ids, timeout=10)
        
        if response.status_code == 200:
            result = response.json()
            deleted_count = len(result.get("deleted", []))
            print_success(f"Xóa thành công {deleted_count} subjects")
            return result
        else:
            print_error(f"Xóa thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi xóa: {str(e)}")
        return None


# ============================================================================
# 6. XÓA TẤT CẢ (Delete All Faces)
# ============================================================================

def delete_all_faces():
    """
    Xóa tất cả subjects (CẢNH BÁO: Không thể hoàn tác!)
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/faces/all"
    
    print_warning("⚠️  CẢNH BÁO: Bạn sắp xóa TẤT CẢ subjects!")
    confirm = input("Nhập 'YES' để xác nhận: ")
    
    if confirm != "YES":
        print_info("Đã hủy thao tác")
        return None
    
    try:
        response = requests.delete(url, timeout=30)
        
        if response.status_code == 200:
            result = response.json()
            deleted_count = result.get("deleted_count", 0)
            print_success(f"Đã xóa {deleted_count} subjects")
            return result
        else:
            print_error(f"Xóa thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi xóa: {str(e)}")
        return None


# ============================================================================
# 7. ĐỔI TÊN SUBJECT (Rename Subject)
# ============================================================================

def rename_subject(old_name, new_name):
    """
    Đổi tên subject
    
    Args:
        old_name: Tên subject hiện tại
        new_name: Tên subject mới
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/subjects/{old_name}"
    data = {"subject": new_name}
    
    try:
        response = requests.put(url, json=data, timeout=10)
        
        if response.status_code == 200:
            result = response.json()
            print_success(f"Đổi tên thành công: {old_name} → {new_name}")
            return result
        else:
            print_error(f"Đổi tên thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi đổi tên: {str(e)}")
        return None


# ============================================================================
# 8. TÌM KIẾM KHUÔN MẶT (Search Face)
# ============================================================================

def search_face(image_path, threshold=0.5, limit=10, det_threshold=0.5):
    """
    Tìm kiếm khuôn mặt tương tự trong database
    
    Args:
        image_path: Đường dẫn đến file ảnh
        threshold: Ngưỡng similarity tối thiểu (0.0-1.0)
        limit: Số lượng kết quả tối đa (0 = không giới hạn)
        det_threshold: Ngưỡng phát hiện khuôn mặt
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/search"
    params = {
        "threshold": threshold,
        "limit": limit,
        "det_prob_threshold": det_threshold
    }
    
    try:
        with open(image_path, "rb") as f:
            files = {"file": (os.path.basename(image_path), f, "image/jpeg")}
            response = requests.post(url, params=params, files=files, timeout=30)
        
        if response.status_code == 200:
            result = response.json()
            faces_found = result.get("faces_found", 0)
            print_success(f"Tìm thấy {faces_found} khuôn mặt tương tự")
            return result
        else:
            print_error(f"Tìm kiếm thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi tìm kiếm: {str(e)}")
        return None


# ============================================================================
# 9. CẤU HÌNH DATABASE (Configure Database)
# ============================================================================

def configure_database(config):
    """
    Cấu hình database connection
    
    Args:
        config: Dict chứa thông tin cấu hình database
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/face-database/connection"
    
    try:
        response = requests.post(url, json=config, timeout=10)
        
        if response.status_code == 200:
            result = response.json()
            print_success(f"Cấu hình database thành công: {result.get('message', '')}")
            return result
        else:
            print_error(f"Cấu hình thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi cấu hình: {str(e)}")
        return None


def get_database_config():
    """
    Lấy cấu hình database hiện tại
    
    Returns:
        dict: Response từ API
    """
    url = f"{BASE_URL}/face-database/connection"
    
    try:
        response = requests.get(url, timeout=10)
        
        if response.status_code == 200:
            result = response.json()
            print_success(f"Cấu hình database: {json.dumps(result, indent=2)}")
            return result
        else:
            print_error(f"Lấy cấu hình thất bại ({response.status_code}): {response.text}")
            return None
    except Exception as e:
        print_error(f"Lỗi khi lấy cấu hình: {str(e)}")
        return None


# ============================================================================
# TEST FUNCTIONS
# ============================================================================

def test_face_detection_validation():
    """
    Test tính năng Face Detection Node:
    - Test với ảnh có khuôn mặt (thành công)
    - Test với ảnh không có khuôn mặt (bị từ chối)
    """
    print_section("TEST: Face Detection Validation")
    
    print_info("Test này kiểm tra tính năng Face Detection Node:")
    print_info("  - Ảnh có khuôn mặt → Đăng ký thành công")
    print_info("  - Ảnh không có khuôn mặt → Bị từ chối với lỗi 400")
    
    # Test 1: Ảnh có khuôn mặt (nếu có file)
    test_image_with_face = input("\nNhập đường dẫn ảnh CÓ khuôn mặt (hoặc Enter để bỏ qua): ").strip()
    if test_image_with_face and os.path.exists(test_image_with_face):
        print_info(f"\nTest 1: Đăng ký ảnh CÓ khuôn mặt: {test_image_with_face}")
        result = register_face_multipart("test_face_detection", test_image_with_face, det_threshold=0.5)
        if result:
            print_success("✓ Test 1 PASSED: Ảnh có khuôn mặt được chấp nhận")
        else:
            print_error("✗ Test 1 FAILED: Ảnh có khuôn mặt bị từ chối")
    else:
        print_warning("Bỏ qua Test 1: Không có file ảnh")
    
    # Test 2: Ảnh không có khuôn mặt
    test_image_no_face = input("\nNhập đường dẫn ảnh KHÔNG có khuôn mặt (landscape, object, etc.) (hoặc Enter để bỏ qua): ").strip()
    if test_image_no_face and os.path.exists(test_image_no_face):
        print_info(f"\nTest 2: Đăng ký ảnh KHÔNG có khuôn mặt: {test_image_no_face}")
        result = register_face_multipart("test_no_face", test_image_no_face, det_threshold=0.5)
        if result is None:
            print_success("✓ Test 2 PASSED: Ảnh không có khuôn mặt bị từ chối đúng cách")
        else:
            print_error("✗ Test 2 FAILED: Ảnh không có khuôn mặt lại được chấp nhận")
    else:
        print_warning("Bỏ qua Test 2: Không có file ảnh")


def run_full_test_workflow():
    """
    Chạy workflow test đầy đủ
    """
    print_section("FULL TEST WORKFLOW")
    
    # 1. Liệt kê subjects hiện tại
    print_info("Bước 1: Liệt kê subjects hiện tại")
    list_result = list_faces(page=0, size=20)
    if list_result:
        print(json.dumps(list_result, indent=2, ensure_ascii=False))
    
    # 2. Đăng ký khuôn mặt mới
    print_info("\nBước 2: Đăng ký khuôn mặt mới")
    test_image = input("Nhập đường dẫn ảnh để đăng ký: ").strip()
    if test_image and os.path.exists(test_image):
        subject_name = input("Nhập tên subject: ").strip() or "test_subject"
        register_result = register_face_multipart(subject_name, test_image)
        
        if register_result:
            image_id = register_result.get("image_id")
            subject = register_result.get("subject")
            
            # 3. Nhận diện khuôn mặt
            print_info("\nBước 3: Nhận diện khuôn mặt")
            recognize_image = input("Nhập đường dẫn ảnh để nhận diện (hoặc Enter để dùng ảnh vừa đăng ký): ").strip()
            if not recognize_image:
                recognize_image = test_image
            
            if os.path.exists(recognize_image):
                recognize_result = recognize_face(recognize_image)
                if recognize_result:
                    print(json.dumps(recognize_result, indent=2, ensure_ascii=False))
            
            # 4. Tìm kiếm khuôn mặt
            print_info("\nBước 4: Tìm kiếm khuôn mặt")
            search_image = input("Nhập đường dẫn ảnh để tìm kiếm (hoặc Enter để dùng ảnh vừa đăng ký): ").strip()
            if not search_image:
                search_image = test_image
            
            if os.path.exists(search_image):
                search_result = search_face(search_image, threshold=0.5)
                if search_result:
                    print(json.dumps(search_result, indent=2, ensure_ascii=False))
            
            # 5. Xóa subject vừa tạo (optional)
            print_info("\nBước 5: Xóa subject (optional)")
            delete_confirm = input(f"Bạn có muốn xóa subject '{subject}' vừa tạo? (y/n): ").strip().lower()
            if delete_confirm == 'y':
                delete_face(image_id)
        else:
            print_error("Không thể đăng ký khuôn mặt, bỏ qua các bước tiếp theo")
    else:
        print_error("File ảnh không tồn tại")


# ============================================================================
# MAIN
# ============================================================================

def main():
    """
    Main function - Interactive menu
    """
    print(f"{Colors.BOLD}{'='*60}{Colors.RESET}")
    print(f"{Colors.BOLD}Recognition API Test Script{Colors.RESET}")
    print(f"{Colors.BOLD}{'='*60}{Colors.RESET}")
    print(f"\nBase URL: {BASE_URL}\n")
    
    while True:
        print("\n" + "="*60)
        print("MENU:")
        print("1. Đăng ký khuôn mặt (Multipart)")
        print("2. Đăng ký khuôn mặt (Base64)")
        print("3. Nhận diện khuôn mặt")
        print("4. Liệt kê subjects")
        print("5. Xóa subject")
        print("6. Xóa nhiều subjects")
        print("7. Đổi tên subject")
        print("8. Tìm kiếm khuôn mặt")
        print("9. Cấu hình database")
        print("10. Lấy cấu hình database")
        print("11. Test Face Detection Validation")
        print("12. Chạy Full Test Workflow")
        print("0. Thoát")
        print("="*60)
        
        choice = input("\nChọn chức năng (0-12): ").strip()
        
        if choice == "0":
            print_info("Thoát chương trình")
            break
        elif choice == "1":
            subject = input("Tên subject: ").strip()
            image_path = input("Đường dẫn ảnh: ").strip()
            if subject and image_path:
                register_face_multipart(subject, image_path)
        elif choice == "2":
            subject = input("Tên subject: ").strip()
            image_path = input("Đường dẫn ảnh: ").strip()
            if subject and image_path:
                register_face_base64(subject, image_path)
        elif choice == "3":
            image_path = input("Đường dẫn ảnh: ").strip()
            if image_path:
                recognize_face(image_path)
        elif choice == "4":
            page = input("Số trang (mặc định 0): ").strip() or "0"
            size = input("Số lượng mỗi trang (mặc định 20): ").strip() or "20"
            subject_filter = input("Lọc theo subject (Enter để bỏ qua): ").strip() or None
            list_faces(int(page), int(size), subject_filter)
        elif choice == "5":
            image_id = input("Image ID hoặc subject name: ").strip()
            if image_id:
                delete_face(image_id)
        elif choice == "6":
            ids_input = input("Danh sách image IDs (phân cách bằng dấu phẩy): ").strip()
            if ids_input:
                image_ids = [id.strip() for id in ids_input.split(",")]
                delete_multiple_faces(image_ids)
        elif choice == "7":
            old_name = input("Tên subject hiện tại: ").strip()
            new_name = input("Tên subject mới: ").strip()
            if old_name and new_name:
                rename_subject(old_name, new_name)
        elif choice == "8":
            image_path = input("Đường dẫn ảnh: ").strip()
            threshold = input("Threshold (mặc định 0.5): ").strip() or "0.5"
            if image_path:
                search_face(image_path, float(threshold))
        elif choice == "9":
            print("\nChọn loại database:")
            print("1. MySQL")
            print("2. PostgreSQL")
            print("3. Tắt database (dùng file)")
            db_choice = input("Chọn (1-3): ").strip()
            
            if db_choice == "1":
                config = {
                    "type": "mysql",
                    "host": input("Host (mặc định localhost): ").strip() or "localhost",
                    "port": int(input("Port (mặc định 3306): ").strip() or "3306"),
                    "database": input("Database name: ").strip(),
                    "username": input("Username: ").strip(),
                    "password": input("Password: ").strip(),
                    "charset": input("Charset (mặc định utf8mb4): ").strip() or "utf8mb4"
                }
                configure_database(config)
            elif db_choice == "2":
                config = {
                    "type": "postgresql",
                    "host": input("Host (mặc định localhost): ").strip() or "localhost",
                    "port": int(input("Port (mặc định 5432): ").strip() or "5432"),
                    "database": input("Database name: ").strip(),
                    "username": input("Username: ").strip(),
                    "password": input("Password: ").strip()
                }
                configure_database(config)
            elif db_choice == "3":
                configure_database({"enabled": False})
        elif choice == "10":
            get_database_config()
        elif choice == "11":
            test_face_detection_validation()
        elif choice == "12":
            run_full_test_workflow()
        else:
            print_warning("Lựa chọn không hợp lệ")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n" + Colors.YELLOW + "Đã hủy bởi người dùng" + Colors.RESET)
        sys.exit(0)
    except Exception as e:
        print_error(f"Lỗi không mong đợi: {str(e)}")
        sys.exit(1)

