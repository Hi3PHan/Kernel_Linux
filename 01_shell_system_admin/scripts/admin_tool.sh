#!/usr/bin/env bash

set -u

APP_NAME="Shell System Admin Lab"
CRON_TAG="LAB_ADMIN_TOOL"
ONCE_TAG="LAB_ADMIN_TOOL_ONCE"
LOG_DIR="${HOME}/.shell_system_admin_lab"
LOG_FILE="${LOG_DIR}/admin_tool.log"

mkdir -p "$LOG_DIR"

# Ghi lai lich su thao tac vao file log
log_action() {
    printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$LOG_FILE"
}

# Dung man hinh cho nguoi dung bam Enter
pause_screen() {
    printf '\nNhan Enter de tiep tuc...'
    read -r _
}

# In tieu de ung dung tren cung man hinh
print_header() {
    clear
    printf '========================================\n'
    printf '%s\n' "$APP_NAME"
    printf '========================================\n'
}

# Bat buoc nguoi dung nhap du lieu (khong duoc de trong)
read_required() {
    local prompt="$1"
    local value

    while true; do
        read -r -p "$prompt" value
        if [ -n "$value" ]; then
            printf '%s' "$value"
            return 0
        fi
        printf 'Gia tri khong duoc de trong.\n' >&2
    done
}

# Hien thi cau hoi xac nhan Yes/No
confirm() {
    local prompt="$1"
    local answer

    read -r -p "$prompt [y/N]: " answer
    case "$answer" in
        y|Y|yes|YES) return 0 ;;
        *) return 1 ;;
    esac
}

# Kiem tra xem 1 lenh/chuong trinh da duoc cai dat tren may chua
need_cmd() {
    local cmd="$1"

    if ! command -v "$cmd" >/dev/null 2>&1; then
        printf 'Lenh: %s khong ton tai.\n' "$cmd"
        return 1
    fi
}

require_non_empty() {
    local label="$1"
    local value="$2"

    if [ -z "$value" ]; then
        printf '%s khong duoc de trong.\n' "$label" >&2
        return 1
    fi
}

absolute_existing_path() {
    local path="$1"
    local base dir

    if [ -d "$path" ]; then
        (cd -- "$path" 2>/dev/null && pwd -P) || return 1
        return 0
    fi

    dir="$(dirname -- "$path")"
    base="$(basename -- "$path")"
    (cd -- "$dir" 2>/dev/null && printf '%s/%s\n' "$(pwd -P)" "$base") || return 1
}

destination_parent_exists() {
    local path="$1"
    local dir

    if [ -e "$path" ]; then
        return 0
    fi

    dir="$(dirname -- "$path")"
    if [ ! -d "$dir" ]; then
        printf 'Thu muc cha khong ton tai: %s\n' "$dir" >&2
        return 1
    fi
}

final_destination_path() {
    local src="$1"
    local dst="$2"
    local dst_dir_abs dst_base

    if [ -d "$dst" ]; then
        dst_dir_abs="$(absolute_existing_path "$dst")" || return 1
        dst_base="$(basename -- "$src")"
    else
        destination_parent_exists "$dst" || return 1
        dst_dir_abs="$(absolute_existing_path "$(dirname -- "$dst")")" || return 1
        dst_base="$(basename -- "$dst")"
    fi

    printf '%s/%s\n' "$dst_dir_abs" "$dst_base"
}

is_path_inside() {
    local parent="$1"
    local child="$2"

    [ "$parent" != "/" ] && [ "$child" != "$parent" ] && [ "${child#"$parent/"}" != "$child" ]
}

reject_dangerous_delete_path() {
    local path="$1"
    local normalized cwd

    normalized="$(absolute_existing_path "$path")" || {
        printf 'Khong the xac dinh duong dan an toan: %s\n' "$path" >&2
        return 1
    }
    cwd="$(pwd -P)"

    case "$normalized" in
        /|"$HOME"|"$cwd")
            printf 'Tu choi xoa duong dan nguy hiem: %s\n' "$normalized" >&2
            return 1
            ;;
    esac
}

# Cac helper core ben duoi nhan tham so truc tiep de menu tuong tac va --cmd dung chung.
file_list_path() {
    local path="$1"

    require_non_empty "Thu muc" "$path" || return 1
    if [ ! -d "$path" ]; then
        printf 'Thu muc khong ton tai: %s\n' "$path" >&2
        return 1
    fi

    ls -lah -- "$path"
    log_action "LIST path=$path"
}

file_stat_path() {
    local path="$1"

    require_non_empty "Duong dan" "$path" || return 1
    need_cmd stat || return 1
    if [ ! -e "$path" ]; then
        printf 'Duong dan khong ton tai: %s\n' "$path" >&2
        return 1
    fi

    stat -- "$path"
    log_action "STAT path=$path"
}

file_read_file_path() {
    local path="$1"

    require_non_empty "File" "$path" || return 1
    if [ ! -f "$path" ]; then
        printf 'Khong phai file hop le: %s\n' "$path" >&2
        return 1
    fi
    if [ ! -r "$path" ]; then
        printf 'Khong co quyen doc file: %s\n' "$path" >&2
        return 1
    fi

    head -n 200 -- "$path"
    log_action "READ_FILE path=$path"
}

file_create_file_path() {
    local path="$1"

    require_non_empty "Duong dan file" "$path" || return 1
    if [ -e "$path" ]; then
        printf 'File/thu muc da ton tai: %s\n' "$path" >&2
        return 1
    fi

    destination_parent_exists "$path" || return 1
    touch -- "$path" || return 1
    printf 'Da tao file: %s\n' "$path"
    log_action "CREATE_FILE path=$path"
}

file_create_dir_path() {
    local path="$1"

    require_non_empty "Duong dan thu muc" "$path" || return 1
    mkdir -p -- "$path" || return 1
    printf 'Da tao thu muc: %s\n' "$path"
    log_action "CREATE_DIR path=$path"
}

file_copy_paths() {
    local src="$1"
    local dst="$2"
    local src_abs target_abs

    require_non_empty "Nguon" "$src" || return 1
    require_non_empty "Dich" "$dst" || return 1
    if [ ! -e "$src" ]; then
        printf 'Nguon khong ton tai: %s\n' "$src" >&2
        return 1
    fi

    target_abs="$(final_destination_path "$src" "$dst")" || return 1
    src_abs="$(absolute_existing_path "$src")" || return 1
    if [ "$src_abs" = "$target_abs" ]; then
        printf 'Tu choi copy len chinh file/thu muc nguon.\n' >&2
        return 1
    fi
    if [ -d "$src" ]; then
        if is_path_inside "$src_abs" "$target_abs"; then
            printf 'Tu choi copy thu muc vao ben trong chinh no.\n' >&2
            return 1
        fi
    fi

    cp -a -- "$src" "$dst" || return 1
    printf 'Da copy "%s" -> "%s"\n' "$src" "$dst"
    log_action "COPY src=$src dst=$dst"
}

file_move_paths() {
    local src="$1"
    local dst="$2"
    local src_abs target_abs

    require_non_empty "Nguon" "$src" || return 1
    require_non_empty "Dich" "$dst" || return 1
    if [ ! -e "$src" ]; then
        printf 'Nguon khong ton tai: %s\n' "$src" >&2
        return 1
    fi

    target_abs="$(final_destination_path "$src" "$dst")" || return 1
    src_abs="$(absolute_existing_path "$src")" || return 1
    if [ "$src_abs" = "$target_abs" ]; then
        printf 'Tu choi move/rename len chinh file/thu muc nguon.\n' >&2
        return 1
    fi
    if [ -d "$src" ]; then
        if is_path_inside "$src_abs" "$target_abs"; then
            printf 'Tu choi move thu muc vao ben trong chinh no.\n' >&2
            return 1
        fi
    fi

    mv -- "$src" "$dst" || return 1
    printf 'Da move/rename "%s" -> "%s"\n' "$src" "$dst"
    log_action "MOVE src=$src dst=$dst"
}

file_delete_path() {
    local path="$1"

    require_non_empty "Duong dan" "$path" || return 1
    if [ ! -e "$path" ]; then
        printf 'Duong dan khong ton tai: %s\n' "$path" >&2
        return 1
    fi

    reject_dangerous_delete_path "$path" || return 1
    rm -rf -- "$path" || return 1
    printf 'Da xoa: %s\n' "$path"
    log_action "DELETE path=$path"
}

file_find_in_dir() {
    local dir="$1"
    local pattern="$2"
    local name_pattern

    require_non_empty "Thu muc" "$dir" || return 1
    if [ ! -d "$dir" ]; then
        printf 'Thu muc khong ton tai: %s\n' "$dir" >&2
        return 1
    fi
    if [ -z "$pattern" ]; then
        printf 'Mau tim kiem khong duoc de trong.\n' >&2
        return 1
    fi

    case "$pattern" in
        *[\*\?\[]*) name_pattern="$pattern" ;;
        *) name_pattern="*$pattern*" ;;
    esac

    find -- "$dir" -iname "$name_pattern" -print
    log_action "FIND dir=$dir pattern=$pattern"
}

file_backup_dir() {
    local src="$1"
    local dst="$2"
    local name archive src_abs dst_abs parent

    require_non_empty "Thu muc nguon" "$src" || return 1
    require_non_empty "Thu muc luu backup" "$dst" || return 1
    need_cmd tar || return 1
    if [ ! -d "$src" ]; then
        printf 'Thu muc nguon khong ton tai: %s\n' "$src" >&2
        return 1
    fi

    src_abs="$(absolute_existing_path "$src")" || return 1
    if [ "$src_abs" = "/" ]; then
        printf 'Tu choi backup toan bo thu muc goc /.\n' >&2
        return 1
    fi

    mkdir -p -- "$dst" || return 1
    dst_abs="$(absolute_existing_path "$dst")" || return 1
    if [ "$dst_abs" = "$src_abs" ] || is_path_inside "$src_abs" "$dst_abs"; then
        printf 'Thu muc luu backup khong duoc nam ben trong thu muc nguon.\n' >&2
        return 1
    fi

    name="$(basename -- "$src_abs")"
    parent="$(dirname -- "$src_abs")"
    archive="${dst_abs}/${name}_$(date '+%Y%m%d_%H%M%S').tar.gz"
    tar -czf "$archive" -C "$parent" -- "$name" || return 1
    printf 'Da tao backup: %s\n' "$archive"
    log_action "BACKUP src=$src archive=$archive"
}

# Liet ke danh sach file va thu muc con
file_list() {
    local path
    path="$(read_required 'Nhap thu muc can liet ke: ')"
    file_list_path "$path"
}

file_stat() {
    local path
    path="$(read_required 'Nhap file/thu muc can xem thong tin: ')"
    file_stat_path "$path"
}

file_read_file() {
    local path
    path="$(read_required 'Nhap file text can doc: ')"
    file_read_file_path "$path"
}

# Tao file hoac thu muc moi
file_create() {
    local type path

    printf '1. Tao file\n'
    printf '2. Tao thu muc\n'
    read -r -p 'Chon: ' type
    path="$(read_required 'Nhap duong dan can tao: ')"

    case "$type" in
        1)
            file_create_file_path "$path"
            ;;
        2)
            file_create_dir_path "$path"
            ;;
        *)
            printf 'Lua chon khong hop le.\n'
            return 1
            ;;
    esac
}

# Copy file hoac thu muc
file_copy() {
    local src dst

    src="$(read_required 'Nhap nguon can copy: ')"
    dst="$(read_required 'Nhap dich: ')"
    file_copy_paths "$src" "$dst"
}

# Di chuyen (hoac doi ten) file/thu muc
file_move() {
    local src dst

    src="$(read_required 'Nhap nguon can move/rename: ')"
    dst="$(read_required 'Nhap dich/ten moi: ')"
    file_move_paths "$src" "$dst"
}

# Xoa file hoac thu muc an toan (co hoi xac nhan)
file_delete() {
    local path

    path="$(read_required 'Nhap file/thu muc can xoa: ')"

    if confirm "Ban chac chan muon xoa $path?"; then
        file_delete_path "$path"
    else
        printf 'Da huy thao tac xoa.\n'
    fi
}

# Tim kiem file theo ten trong 1 thu muc
file_find() {
    local dir pattern

    read -r -p 'Nhap thu muc tim kiem (Enter de dung thu muc hien tai): ' dir
    dir="${dir:-.}"
    pattern="$(read_required 'Nhap ten file can tim: ')"

    file_find_in_dir "$dir" "$pattern"
}

# Nen va sao luu 1 thu muc thanh file .tar.gz
file_backup() {
    local src dst

    src="$(read_required 'Nhap thu muc can backup: ')"
    dst="$(read_required 'Nhap thu muc luu file backup: ')"
    file_backup_dir "$src" "$dst"
}

# Menu rieng cho phan Quan ly file
file_menu() {
    local choice

    while true; do
        print_header
        printf 'Quan ly file\n'
        printf '1. Liet ke thu muc\n'
        printf '2. Xem thong tin file/thu muc\n'
        printf '3. Doc file text\n'
        printf '4. Tao file/thu muc\n'
        printf '5. Copy file/thu muc\n'
        printf '6. Move/rename file/thu muc\n'
        printf '7. Xoa file/thu muc\n'
        printf '8. Tim file\n'
        printf '9. Backup thu muc thanh tar.gz\n'
        printf '0. Quay lai\n'
        read -r -p 'Chon: ' choice

        case "$choice" in
            1) file_list; pause_screen ;;
            2) file_stat; pause_screen ;;
            3) file_read_file; pause_screen ;;
            4) file_create; pause_screen ;;
            5) file_copy; pause_screen ;;
            6) file_move; pause_screen ;;
            7) file_delete; pause_screen ;;
            8) file_find; pause_screen ;;
            9) file_backup; pause_screen ;;
            0) return 0 ;;
            *) printf 'Lua chon khong hop le.\n'; pause_screen ;;
        esac
    done
}

# Lay danh sach cac tac vu cron hien co cua nguoi dung
cron_current() {
    crontab -l 2>/dev/null || true
}

schedule_add_cron_job() {
    local name="$1"
    local expr="$2"
    local command="$3"
    local tmp

    need_cmd crontab || return 1
    tmp="$(mktemp)"

    cron_current > "$tmp"
    printf '%s %s # %s:%s\n' "$expr" "$command" "$CRON_TAG" "$name" >> "$tmp"
    crontab "$tmp" || {
        rm -f "$tmp"
        return 1
    }
    rm -f "$tmp"

    printf 'Da them cron job: %s\n' "$name"
    log_action "CRON_ADD name=$name expr=$expr command=$command"
}

schedule_list_cron_jobs() {
    need_cmd crontab || return 1

    printf 'Cac cron job cua lab:\n'
    cron_current | grep "# ${CRON_TAG}:" || printf 'Chua co job nao.\n'
    log_action "CRON_LIST"
}

schedule_remove_cron_job() {
    local name="$1"
    local tmp

    need_cmd crontab || return 1
    tmp="$(mktemp)"

    cron_current | grep -v "# ${CRON_TAG}:${name}$" > "$tmp"
    crontab "$tmp" || {
        rm -f "$tmp"
        return 1
    }
    rm -f "$tmp"

    printf 'Da xoa neu ton tai cron job: %s\n' "$name"
    log_action "CRON_REMOVE name=$name"
}

once_add_job() {
    local name="$1"
    local time_spec="$2"
    local command="$3"
    local tmp

    need_cmd at || return 1
    tmp="$(mktemp)"
    {
        printf '# %s:%s\n' "$ONCE_TAG" "$name"
        printf '%s\n' "$command"
    } > "$tmp"

    if at "$time_spec" < "$tmp"; then
        rm -f "$tmp"
        printf 'Da them one-shot job: %s\n' "$name"
        log_action "ONCE_ADD name=$name time=$time_spec command=$command"
        return 0
    fi

    rm -f "$tmp"
    return 1
}

once_list_jobs() {
    local id found=0

    need_cmd atq || return 1
    need_cmd at || return 1

    printf 'Cac one-shot job cua lab:\n'
    while read -r id _; do
        [ -n "$id" ] || continue
        if at -c "$id" 2>/dev/null | grep -q "# ${ONCE_TAG}:"; then
            atq | awk -v job_id="$id" '$1 == job_id { print }'
            found=1
        fi
    done <<EOF
$(atq)
EOF

    if [ "$found" -eq 0 ]; then
        printf 'Chua co job nao.\n'
    fi
    log_action "ONCE_LIST"
}

once_remove_job() {
    local job_id="$1"

    need_cmd atrm || return 1
    atrm "$job_id" || return 1
    printf 'Da xoa one-shot job: %s\n' "$job_id"
    log_action "ONCE_REMOVE job_id=$job_id"
}

# Them 1 tac vu tu dong (cron job) moi
schedule_add_cron() {
    local name expr command

    name="$(read_required 'Nhap ten job: ')"
    expr="$(read_required 'Nhap lich cron (VD: */5 * * * *): ')"
    command="$(read_required 'Nhap lenh can chay: ')"
    schedule_add_cron_job "$name" "$expr" "$command"
}

# Xem cac tac vu tu dong do cong cu nay tao ra
schedule_list_cron() {
    schedule_list_cron_jobs
}

# Xoa 1 tac vu tu dong theo ten
schedule_remove_cron() {
    local name

    name="$(read_required 'Nhap ten job can xoa: ')"
    schedule_remove_cron_job "$name"
}

schedule_add_once() {
    local name time_spec command

    name="$(read_required 'Nhap ten one-shot job: ')"
    time_spec="$(read_required 'Nhap thoi diem at (VD: now + 5 minutes): ')"
    command="$(read_required 'Nhap lenh can chay: ')"
    once_add_job "$name" "$time_spec" "$command"
}

schedule_list_once() {
    once_list_jobs
}

schedule_remove_once() {
    local job_id

    job_id="$(read_required 'Nhap at job id can xoa: ')"
    if confirm "Ban chac chan muon xoa one-shot job $job_id?"; then
        once_remove_job "$job_id"
    else
        printf 'Da huy thao tac xoa.\n'
    fi
}

# Menu rieng cho phan Lap lich tac vu
schedule_menu() {
    local choice

    while true; do
        print_header
        printf 'Lap lich tac vu\n'
        printf '1. Them cron job\n'
        printf '2. Liet ke cron job cua lab\n'
        printf '3. Xoa cron job theo ten\n'
        printf '4. Them one-shot job bang at\n'
        printf '5. Liet ke one-shot job cua lab\n'
        printf '6. Xoa one-shot job theo id\n'
        printf '0. Quay lai\n'
        read -r -p 'Chon: ' choice

        case "$choice" in
            1) schedule_add_cron; pause_screen ;;
            2) schedule_list_cron; pause_screen ;;
            3) schedule_remove_cron; pause_screen ;;
            4) schedule_add_once; pause_screen ;;
            5) schedule_list_once; pause_screen ;;
            6) schedule_remove_once; pause_screen ;;
            0) return 0 ;;
            *) printf 'Lua chon khong hop le.\n'; pause_screen ;;
        esac
    done
}

# Xem ngay gio va mui gio hien tai
time_show() {
    date
    if command -v timedatectl >/dev/null 2>&1; then
        timedatectl
    fi
    log_action "TIME_SHOW"
}

time_set_timezone_value() {
    local zone="$1"

    need_cmd timedatectl || return 1
    sudo timedatectl set-timezone "$zone" || return 1
    printf 'Da doi timezone thanh: %s\n' "$zone"
    log_action "TIMEZONE_SET zone=$zone"
}

time_set_datetime_value() {
    local value="$1"

    need_cmd timedatectl || return 1
    sudo timedatectl set-ntp false || return 1
    sudo timedatectl set-time "$value" || return 1
    printf 'Da cap nhat thoi gian he thong.\n'
    log_action "TIME_SET value=$value"
}

time_enable_ntp_core() {
    need_cmd timedatectl || return 1
    sudo timedatectl set-ntp true || return 1
    printf 'Da bat dong bo thoi gian NTP.\n'
    log_action "NTP_ENABLE"
}

# Thay doi mui gio (Timezone) cua he thong
time_set_timezone() {
    local zone

    zone="$(read_required 'Nhap timezone (VD: Asia/Ho_Chi_Minh): ')"
    time_set_timezone_value "$zone"
}

# Cai dat thu cong ngay gio cho may tinh
time_set_datetime() {
    local value

    value="$(read_required 'Nhap thoi gian (YYYY-MM-DD HH:MM:SS): ')"
    time_set_datetime_value "$value"
}

# Bat dong bo gio tu dong qua mang internet (NTP)
time_enable_ntp() {
    time_enable_ntp_core
}

# Menu rieng cho phan Thiet lap thoi gian
time_menu() {
    local choice

    while true; do
        print_header
        printf 'Thiet lap thoi gian he thong\n'
        printf '1. Xem thoi gian hien tai\n'
        printf '2. Doi timezone\n'
        printf '3. Dat ngay gio he thong\n'
        printf '4. Bat dong bo NTP\n'
        printf '0. Quay lai\n'
        read -r -p 'Chon: ' choice

        case "$choice" in
            1) time_show; pause_screen ;;
            2) time_set_timezone; pause_screen ;;
            3) time_set_datetime; pause_screen ;;
            4) time_enable_ntp; pause_screen ;;
            0) return 0 ;;
            *) printf 'Lua chon khong hop le.\n'; pause_screen ;;
        esac
    done
}

# Tu dong nhan dien cong cu quan ly phan mem (apt, dnf, yum, pacman)
detect_package_manager() {
    if command -v apt-get >/dev/null 2>&1; then
        printf 'apt'
    elif command -v dnf >/dev/null 2>&1; then
        printf 'dnf'
    elif command -v yum >/dev/null 2>&1; then
        printf 'yum'
    elif command -v pacman >/dev/null 2>&1; then
        printf 'pacman'
    else
        printf 'unknown'
    fi
}

# Cap nhat danh sach cac goi phan mem (update cache)
pkg_update() {
    case "$(detect_package_manager)" in
        apt) sudo apt-get update || return 1 ;;
        dnf) sudo dnf makecache || return 1 ;;
        yum) sudo yum makecache || return 1 ;;
        pacman) sudo pacman -Sy || return 1 ;;
        *) printf 'Khong tim thay package manager ho tro.\n'; return 1 ;;
    esac
    printf 'Da cap nhat package index/cache.\n'
    log_action "PKG_UPDATE"
}

# Cai dat nhieu phan mem cung luc tu 1 danh sach truyen vao
pkg_install_list() {
    local packages="$1"
    local -a package_array

    if [ -z "$packages" ]; then
        printf 'Danh sach package rong.\n'
        return 1
    fi

    read -r -a package_array <<< "$packages"
    pkg_install_args "${package_array[@]}"
}

# Go bo nhieu phan mem cung luc tu 1 danh sach truyen vao
pkg_remove_list() {
    local packages="$1"
    local -a package_array

    if [ -z "$packages" ]; then
        printf 'Danh sach package rong.\n'
        return 1
    fi

    read -r -a package_array <<< "$packages"
    pkg_remove_args "${package_array[@]}"
}

# Cai dat package tu argv de command mode khong phai noi chuoi shell thu cong
pkg_install_args() {
    if [ "$#" -eq 0 ]; then
        printf 'Danh sach package rong.\n'
        return 1
    fi

    case "$(detect_package_manager)" in
        apt) sudo apt-get install -y "$@" || return 1 ;;
        dnf) sudo dnf install -y "$@" || return 1 ;;
        yum) sudo yum install -y "$@" || return 1 ;;
        pacman) sudo pacman -S --noconfirm "$@" || return 1 ;;
        *) printf 'Khong tim thay package manager ho tro.\n'; return 1 ;;
    esac
    printf 'Da cai package: %s\n' "$*"
    log_action "PKG_INSTALL packages=$*"
}

# Go package tu argv de giu tung package la mot tham so rieng
pkg_remove_args() {
    if [ "$#" -eq 0 ]; then
        printf 'Danh sach package rong.\n'
        return 1
    fi

    case "$(detect_package_manager)" in
        apt) sudo apt-get remove -y "$@" || return 1 ;;
        dnf) sudo dnf remove -y "$@" || return 1 ;;
        yum) sudo yum remove -y "$@" || return 1 ;;
        pacman) sudo pacman -R --noconfirm "$@" || return 1 ;;
        *) printf 'Khong tim thay package manager ho tro.\n'; return 1 ;;
    esac
    printf 'Da go package: %s\n' "$*"
    log_action "PKG_REMOVE packages=$*"
}

# Cai dat phan mem bang cach nhap ten thu cong
pkg_install_manual() {
    local packages

    packages="$(read_required 'Nhap cac package can cai, cach nhau boi dau cach: ')"
    pkg_install_list "$packages"
}

# Doc file text va cai dat toan bo phan mem co trong file do
pkg_install_from_file() {
    local file packages

    file="$(read_required 'Nhap file danh sach package: ')"
    if [ ! -f "$file" ]; then
        printf 'File khong ton tai: %s\n' "$file"
        return 1
    fi

    packages="$(grep -v '^[[:space:]]*#' "$file" | tr '\n' ' ')"
    pkg_install_list "$packages"
}

# Go bo phan mem bang cach nhap ten thu cong
pkg_remove_manual() {
    local packages

    packages="$(read_required 'Nhap cac package can go, cach nhau boi dau cach: ')"
    if confirm "Ban chac chan muon go: $packages?"; then
        pkg_remove_list "$packages"
    else
        printf 'Da huy thao tac go package.\n'
    fi
}

# Menu rieng cho phan Cai dat va go bo phan mem
pkg_menu() {
    local choice manager

    while true; do
        manager="$(detect_package_manager)"
        print_header
        printf 'Cai dat/go bo chuong trinh tu dong\n'
        printf 'Package manager: %s\n' "$manager"
        printf '1. Cap nhat package index/cache\n'
        printf '2. Cai package\n'
        printf '3. Cai package tu file danh sach\n'
        printf '4. Go package\n'
        printf '0. Quay lai\n'
        read -r -p 'Chon: ' choice

        case "$choice" in
            1) pkg_update; pause_screen ;;
            2) pkg_install_manual; pause_screen ;;
            3) pkg_install_from_file; pause_screen ;;
            4) pkg_remove_manual; pause_screen ;;
            0) return 0 ;;
            *) printf 'Lua chon khong hop le.\n'; pause_screen ;;
        esac
    done
}

# Xem 50 dong cuoi cung trong file nhat ky (log)
show_log() {
    if [ -f "$LOG_FILE" ]; then
        tail -n 50 "$LOG_FILE"
    else
        printf 'Chua co log.\n'
    fi
}

cmd_usage() {
    cat <<'EOF'
Usage:
  admin_tool.sh --cmd help
  admin_tool.sh --cmd list <dir>
  admin_tool.sh --cmd stat <path>
  admin_tool.sh --cmd read-file <path>
  admin_tool.sh --cmd create-file <path>
  admin_tool.sh --cmd create-dir <path>
  admin_tool.sh --cmd copy <src> <dst>
  admin_tool.sh --cmd move <src> <dst>
  admin_tool.sh --cmd delete <path> --yes
  admin_tool.sh --cmd find <dir> <pattern>
  admin_tool.sh --cmd backup <src-dir> <dst-dir>
  admin_tool.sh --cmd cron-list
  admin_tool.sh --cmd cron-add <name> <expr> <command>
  admin_tool.sh --cmd cron-remove <name> --yes
  admin_tool.sh --cmd once-add <name> <time-spec> <command>
  admin_tool.sh --cmd once-list
  admin_tool.sh --cmd once-remove <job-id> --yes
  admin_tool.sh --cmd time-show
  admin_tool.sh --cmd timezone-set <zone> --yes
  admin_tool.sh --cmd time-set <YYYY-MM-DD HH:MM:SS> --yes
  admin_tool.sh --cmd ntp-enable --yes
  admin_tool.sh --cmd pkg-update --yes
  admin_tool.sh --cmd pkg-install <pkg> [pkg...] --yes
  admin_tool.sh --cmd pkg-install-file <file> --yes
  admin_tool.sh --cmd pkg-remove <pkg> [pkg...] --yes
  admin_tool.sh --cmd log
EOF
}

cmd_has_yes() {
    local arg

    for arg in "$@"; do
        if [ "$arg" = "--yes" ]; then
            return 0
        fi
    done
    return 1
}

cmd_require_yes() {
    if ! cmd_has_yes "$@"; then
        printf 'Tu choi thao tac nguy hiem vi thieu --yes.\n' >&2
        return 1
    fi
}

cmd_strip_yes() {
    CMD_ARGS=()
    local arg

    for arg in "$@"; do
        if [ "$arg" != "--yes" ]; then
            CMD_ARGS+=("$arg")
        fi
    done
}

cmd_expect_args() {
    local expected="$1"
    shift

    if [ "$#" -ne "$expected" ]; then
        cmd_usage >&2
        return 1
    fi
}

command_mode() {
    local cmd="${1:-help}"
    local path src dst name expr command zone value file job_id time_spec
    local -a packages

    if [ "$#" -gt 0 ]; then
        shift
    fi

    case "$cmd" in
        help|-h|--help)
            cmd_usage
            ;;
        list)
            cmd_expect_args 1 "$@" || return 1
            file_list_path "$1"
            ;;
        stat)
            cmd_expect_args 1 "$@" || return 1
            file_stat_path "$1"
            ;;
        read-file)
            cmd_expect_args 1 "$@" || return 1
            file_read_file_path "$1"
            ;;
        create-file)
            cmd_expect_args 1 "$@" || return 1
            file_create_file_path "$1"
            ;;
        create-dir)
            cmd_expect_args 1 "$@" || return 1
            file_create_dir_path "$1"
            ;;
        copy)
            cmd_expect_args 2 "$@" || return 1
            file_copy_paths "$1" "$2"
            ;;
        move)
            cmd_expect_args 2 "$@" || return 1
            file_move_paths "$1" "$2"
            ;;
        delete)
            cmd_require_yes "$@" || return 1
            cmd_strip_yes "$@"
            cmd_expect_args 1 "${CMD_ARGS[@]}" || return 1
            file_delete_path "${CMD_ARGS[0]}"
            ;;
        find)
            cmd_expect_args 2 "$@" || return 1
            file_find_in_dir "$1" "$2"
            ;;
        backup)
            cmd_expect_args 2 "$@" || return 1
            file_backup_dir "$1" "$2"
            ;;
        cron-list)
            schedule_list_cron_jobs
            ;;
        cron-add)
            cmd_expect_args 3 "$@" || return 1
            name="$1"
            expr="$2"
            command="$3"
            schedule_add_cron_job "$name" "$expr" "$command"
            ;;
        cron-remove)
            cmd_require_yes "$@" || return 1
            cmd_strip_yes "$@"
            cmd_expect_args 1 "${CMD_ARGS[@]}" || return 1
            name="${CMD_ARGS[0]}"
            schedule_remove_cron_job "$name"
            ;;
        once-add)
            cmd_expect_args 3 "$@" || return 1
            name="$1"
            time_spec="$2"
            command="$3"
            once_add_job "$name" "$time_spec" "$command"
            ;;
        once-list)
            cmd_expect_args 0 "$@" || return 1
            once_list_jobs
            ;;
        once-remove)
            cmd_require_yes "$@" || return 1
            cmd_strip_yes "$@"
            cmd_expect_args 1 "${CMD_ARGS[@]}" || return 1
            job_id="${CMD_ARGS[0]}"
            once_remove_job "$job_id"
            ;;
        time-show)
            time_show
            ;;
        timezone-set)
            cmd_require_yes "$@" || return 1
            cmd_strip_yes "$@"
            cmd_expect_args 1 "${CMD_ARGS[@]}" || return 1
            time_set_timezone_value "${CMD_ARGS[0]}"
            ;;
        time-set)
            cmd_require_yes "$@" || return 1
            cmd_strip_yes "$@"
            cmd_expect_args 1 "${CMD_ARGS[@]}" || return 1
            time_set_datetime_value "${CMD_ARGS[0]}"
            ;;
        ntp-enable)
            cmd_require_yes "$@" || return 1
            time_enable_ntp_core
            ;;
        pkg-update)
            cmd_require_yes "$@" || return 1
            pkg_update
            ;;
        pkg-install)
            cmd_require_yes "$@" || return 1
            cmd_strip_yes "$@"
            pkg_install_args "${CMD_ARGS[@]}"
            ;;
        pkg-install-file)
            cmd_require_yes "$@" || return 1
            cmd_strip_yes "$@"
            cmd_expect_args 1 "${CMD_ARGS[@]}" || return 1
            file="${CMD_ARGS[0]}"
            if [ ! -f "$file" ]; then
                printf 'File khong ton tai: %s\n' "$file" >&2
                return 1
            fi
            packages=()
            while IFS= read -r name; do
                case "$name" in
                    ''|\#*) continue ;;
                    *) packages+=("$name") ;;
                esac
            done < "$file"
            pkg_install_args "${packages[@]}"
            ;;
        pkg-remove)
            cmd_require_yes "$@" || return 1
            cmd_strip_yes "$@"
            pkg_remove_args "${CMD_ARGS[@]}"
            ;;
        log)
            show_log
            ;;
        *)
            printf 'Lenh command mode khong hop le: %s\n' "$cmd" >&2
            cmd_usage >&2
            return 1
            ;;
    esac
}

# Trinh don chinh cua toan bo chuong trinh
main_menu() {
    local choice

    while true; do
        print_header
        printf '1. Quan ly file\n'
        printf '2. Lap lich tac vu\n'
        printf '3. Thiet lap thoi gian he thong\n'
        printf '4. Cai dat/go bo chuong trinh tu dong\n'
        printf '5. Xem log thao tac\n'
        printf '0. Thoat\n'
        read -r -p 'Chon: ' choice

        case "$choice" in
            1) file_menu ;;
            2) schedule_menu ;;
            3) time_menu ;;
            4) pkg_menu ;;
            5) show_log; pause_screen ;;
            0) printf 'Tam biet.\n'; exit 0 ;;
            *) printf 'Lua chon khong hop le.\n'; pause_screen ;;
        esac
    done
}

if [ "${1:-}" = "--cmd" ]; then
    shift
    command_mode "$@"
    exit $?
fi

main_menu
