import re, json
import pandas as pd

def pqt_to_excel(parquet_path, excel_path):
    df = pd.read_parquet(parquet_path, engine='pyarrow')
    df.to_excel(excel_path, index=False, engine='openpyxl')
    print(f"Done: '{excel_path}'")

input = 'dataset/000<n>.parquet'     # did this for like all parquet files from futo
output = 'excel/output_data<n>.xlsx'
pqt_to_excel(input, output)

def get_key(x, y):
    layout = [
        ['q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'],
        ['a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l'],
        ['z', 'x', 'c', 'v', 'b', 'n', 'm']
    ]
    row_idx = max(0, min(int(y * 3), 2))
    row = layout[row_idx]
    col_idx = max(0, min(int(x * len(row)), len(row) - 1))
    return row[col_idx]

def transform_data(input_file, output_file):
    df = pd.read_excel(input_file)
    results = []
    
    for _, row in df.iterrows():
        raw_str = str(row['data']).replace('\xa0', ' ')
        fixed_str = re.sub(r'}\s*{', '}, {', raw_str)
        
        try:
            points = json.loads(fixed_str.replace("'", '"'))
            sequence = []
            last_key = None
            for pt in points:
                key = get_key(pt['x'], pt['y'])
                if key != last_key:
                    sequence.append(key)
                    last_key = key
            results.append({'word': row['word'], 'sequence': sequence})
        except:
            continue
            
    pd.DataFrame(results).to_excel(output_file, index=False)

transform_data('output_data<n>.xlsx', 'cleaned_data<n>.xlsx')
# then merge the outputs with power query