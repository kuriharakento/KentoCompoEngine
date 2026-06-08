import os
import re
import uuid
import xml.etree.ElementTree as ET
import xml.dom.minidom as minidom

# Windows環境の改行コードに対応するため、オープン時のモードや改行コードに配慮する
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
VCXPROJ_PATH = os.path.join(PROJECT_DIR, "KentoCompo.vcxproj")
FILTERS_PATH = os.path.join(PROJECT_DIR, "KentoCompo.vcxproj.filters")

# スキャン対象フォルダ
TARGET_DIRS = ["engine", "application"]
# 対象拡張子とMSBuildのタグ名マッピング
EXT_MAP = {
    ".cpp": "ClCompile",
    ".h": "ClInclude",
    ".hpp": "ClInclude",
    ".hlsl": "FxCompile",
    ".hlsli": "None"
}

# XML namespace
NS_URI = "http://schemas.microsoft.com/developer/msbuild/2003"
ET.register_namespace('', NS_URI)
NS = {"ns": NS_URI}

def scan_physical_files():
    """
    物理ディレクトリからソース/ヘッダ/シェーダーファイルを再帰的に検索する
    """
    found_files = {}
    for target in TARGET_DIRS:
        target_path = os.path.join(PROJECT_DIR, target)
        if not os.path.exists(target_path):
            continue
        for root, _, filenames in os.walk(target_path):
            for filename in filenames:
                ext = os.path.splitext(filename)[1].lower()
                if ext in EXT_MAP:
                    rel_path = os.path.relpath(os.path.join(root, filename), PROJECT_DIR)
                    rel_path = rel_path.replace('/', '\\')
                    found_files[rel_path] = EXT_MAP[ext]
    return found_files

def parse_existing_xml(file_path):
    """
    XMLファイルをパースしてルートと名前空間情報を取得する
    """
    if not os.path.exists(file_path):
        return None
    # ElementTreeで読み込み
    tree = ET.parse(file_path)
    return tree

def get_existing_uuid_map(tree):
    """
    既存のfiltersファイルからフィルター名とUUIDのマップを取得する
    """
    uuid_map = {}
    if tree is None:
        return uuid_map
    root = tree.getroot()
    # FilterタグからInclude属性とUniqueIdentifierを取得
    for item_group in root.findall("ns:ItemGroup", NS):
        for filter_elem in item_group.findall("ns:Filter", NS):
            name = filter_elem.get("Include")
            ui_elem = filter_elem.find("ns:UniqueIdentifier", NS)
            if name and ui_elem is not None and ui_elem.text:
                uuid_map[name] = ui_elem.text
    return uuid_map

def generate_filter_hierarchies(file_paths):
    """
    ファイルパス一覧から、必要なすべてのフィルター（フォルダ階層）のリストを生成する
    例: "engine\\manager\\editor\\file.cpp" -> ["engine", "engine\\manager", "engine\\manager\\editor"]
    """
    filters = set()
    for path in file_paths:
        dirname = os.path.dirname(path)
        if not dirname:
            continue
        parts = dirname.split('\\')
        for i in range(1, len(parts) + 1):
            filter_path = '\\'.join(parts[:i])
            filters.add(filter_path)
    return sorted(list(filters))

def get_all_registered_files(tree, scan_files):
    """
    vcxprojから既存の登録ファイルを抽出し、スキャン対象ディレクトリ外のファイルを維持しつつ
    スキャン対象ディレクトリ内のファイルを最新のものに置き換える
    """
    # 既存の登録ファイルリストを抽出 (タグ名 -> {相対パス: 要素メタデータ})
    existing_files = {}
    if tree is not None:
        root = tree.getroot()
        for item_group in root.findall("ns:ItemGroup", NS):
            for tag in ["ClCompile", "ClInclude", "FxCompile", "None"]:
                for elem in item_group.findall(f"ns:{tag}", NS):
                    inc = elem.get("Include")
                    if not inc:
                        continue
                    # 属性や子要素（ExcludedFromBuildなど）をディクショナリで保持
                    metadata = {}
                    for child in elem:
                        metadata[child.tag.replace(f"{{{NS_URI}}}", "")] = child.text
                    existing_files[inc] = (tag, metadata)

    # マージ処理
    merged_files = {}
    
    # 1. スキャン対象外の既存登録ファイルを維持 (externals\\ や main.cpp など)
    for inc, (tag, meta) in existing_files.items():
        is_in_target_dir = False
        for target in TARGET_DIRS:
            if inc.startswith(target + '\\'):
                is_in_target_dir = True
                break
        if not is_in_target_dir:
            merged_files[inc] = (tag, meta)
            
    # 2. スキャンで発見した最新のファイルを登録
    for scan_inc, scan_tag in scan_files.items():
        # もし既存のメタデータ（ExcludedFromBuildなど）があれば引き継ぐ
        existing_meta = existing_files.get(scan_inc, (None, {}))[1]
        merged_files[scan_inc] = (scan_tag, existing_meta)
        
    return merged_files

def update_vcxproj(tree, merged_files):
    """
    KentoCompo.vcxproj の ItemGroup を更新する
    """
    root = tree.getroot()
    
    # 既存の ClCompile, ClHeader, FxCompile, None が含まれる ItemGroup を削除する
    # ただし、ProjectReference などが含まれる ItemGroup や、他の要素は削除しないように注意
    groups_to_remove = []
    for item_group in root.findall("ns:ItemGroup", NS):
        has_file_items = False
        for tag in ["ClCompile", "ClInclude", "FxCompile", "None"]:
            if item_group.find(f"ns:{tag}", NS) is not None:
                has_file_items = True
                break
        # ファイルアイテムを含むItemGroupは削除対象としてマーク
        if has_file_items:
            groups_to_remove.append(item_group)
            
    for group in groups_to_remove:
        root.remove(group)
        
    # 新しいItemGroupを追加
    # MSBuildの流儀に合わせて、ClCompile, ClHeader などをそれぞれ別のItemGroupに整理する
    compile_items = {}
    header_items = {}
    shader_items = {}
    other_items = {}
    
    for inc in sorted(merged_files.keys()):
        tag, meta = merged_files[inc]
        if tag == "ClCompile":
            compile_items[inc] = meta
        elif tag == "ClInclude":
            header_items[inc] = meta
        elif tag == "FxCompile":
            shader_items[inc] = meta
        else:
            other_items[inc] = meta
            
    # XML要素の構築
    def create_item_group(items, tag):
        if not items:
            return None
        group = ET.Element("ItemGroup")
        for inc, meta in items.items():
            item = ET.SubElement(group, tag, Include=inc)
            # メタデータの書き込み (ExcludedFromBuild等)
            for k, v in meta.items():
                child = ET.SubElement(item, k)
                child.text = v
        return group

    # 追加順序: ClCompile -> ClHeader -> FxCompile -> None など
    # targetsインポートの直前に挿入したいので、最後のImportタグの前に配置する
    insert_index = len(root)
    for i, child in enumerate(root):
        if child.tag.endswith("Import") and "targets" in child.get("Project", ""):
            insert_index = i
            break
            
    # 追加
    for items, tag in [
        (compile_items, "ClCompile"),
        (header_items, "ClInclude"),
        (shader_items, "FxCompile"),
        (other_items, "None")
    ]:
        group_elem = create_item_group(items, tag)
        if group_elem is not None:
            root.insert(insert_index, group_elem)
            insert_index += 1

def update_filters(tree, merged_files, existing_uuids):
    """
    KentoCompo.vcxproj.filters を更新する
    """
    root = tree.getroot()
    
    # 既存のすべての ItemGroup をクリア（一から再構築する）
    groups_to_remove = list(root.findall("ns:ItemGroup", NS))
    for group in groups_to_remove:
        root.remove(group)
        
    # 1. フィルター定義のItemGroupを作成
    # 登録されている全ファイルパスからフィルター階層を抽出
    filter_paths = generate_filter_hierarchies(merged_files.keys())
    
    filter_group = ET.Element("ItemGroup")
    for fp in filter_paths:
        filter_elem = ET.SubElement(filter_group, "Filter", Include=fp)
        ui_elem = ET.SubElement(filter_elem, "UniqueIdentifier")
        
        # 既存のUUIDがあれば使い回し、無ければ新規生成
        if fp in existing_uuids:
            ui_elem.text = existing_uuids[fp]
        else:
            ui_elem.text = f"{{{str(uuid.uuid4())}}}"
            
    root.append(filter_group)
    
    # 2. ファイルアイテムのItemGroupを作成
    compile_items = []
    header_items = []
    shader_items = []
    other_items = []
    
    for inc in sorted(merged_files.keys()):
        tag, meta = merged_files[inc]
        dirname = os.path.dirname(inc)
        # フィルター要素を作成
        item = ET.Element(tag, Include=inc)
        if dirname:
            filter_child = ET.SubElement(item, "Filter")
            filter_child.text = dirname
            
        if tag == "ClCompile":
            compile_items.append(item)
        elif tag == "ClInclude":
            header_items.append(item)
        elif tag == "FxCompile":
            shader_items.append(item)
        else:
            other_items.append(item)
            
    # それぞれのグループを追加
    for items in [compile_items, header_items, shader_items, other_items]:
        if items:
            group = ET.Element("ItemGroup")
            for item in items:
                group.append(item)
            root.append(group)

def write_xml_file(tree, file_path):
    """
    XMLファイルを綺麗にフォーマットして保存する (Visual Studio標準のインデントを維持)
    """
    # ElementTreeから文字列を取得
    xml_str = ET.tostring(tree.getroot(), encoding='utf-8')
    
    # minidomでパースして整形
    dom = minidom.parseString(xml_str)
    pretty_xml = dom.toprettyxml(indent="  ", encoding="utf-8").decode("utf-8")
    
    # toprettyxmlによって発生する余分な空行を削除
    pretty_xml = re.sub(r'\n\s*\n', '\n', pretty_xml)
    
    # XML宣言部をVisual Studioの流儀に合わせる (<?xml version="1.0" encoding="utf-8"?>)
    # また、 ElementTreeで名前空間接頭辞が無い場合、デフォルト名前空間xmlnsの二重定義を防ぐ
    pretty_xml = re.sub(r'<\?xml[^>]*\?>', '<?xml version="1.0" encoding="utf-8"?>', pretty_xml)
    
    # 保存
    with open(file_path, "w", encoding="utf-8") as f:
        f.write(pretty_xml)
    print(f"Updated: {file_path}")

def main():
    print("Scanning physical files...")
    scan_files = scan_physical_files()
    print(f"Found {len(scan_files)} C++/shader files physically.")

    # 1. vcxprojの更新
    if os.path.exists(VCXPROJ_PATH):
        print("Updating vcxproj...")
        proj_tree = parse_existing_xml(VCXPROJ_PATH)
        if proj_tree:
            merged_files = get_all_registered_files(proj_tree, scan_files)
            update_vcxproj(proj_tree, merged_files)
            write_xml_file(proj_tree, VCXPROJ_PATH)
    else:
        print(f"Error: {VCXPROJ_PATH} not found.")
        return

    # 2. filtersの更新
    if os.path.exists(FILTERS_PATH):
        print("Updating vcxproj.filters...")
        filters_tree = parse_existing_xml(FILTERS_PATH)
        if filters_tree:
            existing_uuids = get_existing_uuid_map(filters_tree)
            # vcxprojと同じマージ後ファイルリストを使用
            update_filters(filters_tree, merged_files, existing_uuids)
            write_xml_file(filters_tree, FILTERS_PATH)
    else:
        # 存在しない場合は新規作成
        print("Creating new vcxproj.filters...")
        root = ET.Element("Project", ToolsVersion="4.0", xmlns=NS_URI)
        filters_tree = ET.ElementTree(root)
        merged_files = get_all_registered_files(None, scan_files)
        update_filters(filters_tree, merged_files, {})
        write_xml_file(filters_tree, FILTERS_PATH)

    print("Project files synchronized successfully!")

if __name__ == "__main__":
    main()
