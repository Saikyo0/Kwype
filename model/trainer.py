import ast
import math
import os
import numpy as np
import pandas as pd

os.environ['TF_GPU_ALLOCATOR'] = 'cuda_malloc_async'
import tensorflow as tf  # type: ignore
from tensorflow.keras import layers # type: ignore
from sklearn.preprocessing import LabelEncoder
from sklearn.model_selection import train_test_split

df = pd.read_excel('dataset/excel/cleaned.xlsx')

def parse_sequence(seq):
    if isinstance(seq, str):
        try: return ast.literal_eval(seq)
        except (ValueError, SyntaxError): return list(seq)
    elif isinstance(seq, list): return seq
    else: return list(str(seq))

sequences = [parse_sequence(s) for s in df['sequence'].tolist()]
target_words = df['word'].tolist()

MAX_LEN = max(len(s) for s in sequences)
print(f"Max sequence length: {MAX_LEN}")

label_encoder = LabelEncoder()
y_encoded = label_encoder.fit_transform(target_words)
num_unique_words = len(label_encoder.classes_)
print(f"Number of unique words: {num_unique_words}")

key_map = {
    'q': (0, 0), 'w': (1, 0), 'e': (2, 0), 'r': (3, 0), 't': (4, 0),
    'y': (5, 0), 'u': (6, 0), 'i': (7, 0), 'o': (8, 0), 'p': (9, 0),
    'a': (0.5, 1), 's': (1.5, 1), 'd': (2.5, 1), 'f': (3.5, 1),
    'g': (4.5, 1), 'h': (5.5, 1), 'j': (6.5, 1), 'k': (7.5, 1), 'l': (8.5, 1),
    'z': (1.5, 2), 'x': (2.5, 2), 'c': (3.5, 2), 'v': (4.5, 2),
    'b': (5.5, 2), 'n': (6.5, 2), 'm': (7.5, 2)
}

def get_coords(seq):
    return [key_map.get(char.lower(), (0, 0)) for char in seq]

def pad_coords(coords_list, max_len):
    num_samples = len(coords_list)
    padded = np.zeros((num_samples, max_len, 2), dtype='float32')
    
    for i, coords in enumerate(coords_list):
        seq_len = min(len(coords), max_len)
        for j in range(seq_len):
            padded[i, j, 0] = coords[j][0]
            padded[i, j, 1] = coords[j][1]
    
    return padded

X_coords = [get_coords(s) for s in sequences]
X_padded = pad_coords(X_coords, MAX_LEN)
print(f"X_padded shape: {X_padded.shape}")
X_train, X_val, y_train, y_val = train_test_split(
    X_padded, y_encoded, test_size=0.2, random_state=42
)

print(f"Training samples: {len(X_train)}, Validation samples: {len(X_val)}")

model = tf.keras.Sequential([
    layers.Input(shape=(MAX_LEN, 2)),
    layers.Conv1D(128, kernel_size=3, padding='same', activation='relu'),
    layers.BatchNormalization(),
    layers.Conv1D(256, kernel_size=3, padding='same', activation='relu'),
    layers.BatchNormalization(),
    layers.GlobalAveragePooling1D(),
    layers.Dense(512, activation='relu'),
    layers.Dropout(0.3),
    layers.Dense(256, activation='relu'),
    layers.Dropout(0.2),
    layers.Dense(num_unique_words, activation='softmax')
])

model.compile(
    optimizer='adam',
    loss='sparse_categorical_crossentropy',
    metrics=['accuracy']
)
model.summary()

early_stopping = tf.keras.callbacks.EarlyStopping(
    monitor='val_loss',
    patience=5,
    restore_best_weights=True
)

history = model.fit(
    X_train, y_train,
    validation_data=(X_val, y_val),
    epochs=50,
    batch_size=128,
    callbacks=[early_stopping]
)

def predict_swype(input_chars):
    if isinstance(input_chars, str): input_chars = list(input_chars)
    coords = get_coords(input_chars)
    padded_input = pad_coords([coords], MAX_LEN)
    prediction = model.predict(padded_input, verbose=0)
    
    top_indices = np.argsort(prediction[0])[-3:][::-1]
    top_words = label_encoder.inverse_transform(top_indices)
    top_probs = prediction[0][top_indices]
    
    return list(zip(top_words, top_probs))

print("keras model generated")
model.save('model/my_model.keras')

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.target_spec.supported_types = [tf.float16]
tflite_model = converter.convert()
output_path = 'assets/data/model_quant.tflite'
with open(output_path, 'wb') as f:
    f.write(tflite_model)
print("tflite model generated")

np.save("data/label_classes.npy", label_encoder.classes_)
np.save("data/max_len.npy", MAX_LEN)

# the npy files are for generating py exes
# cpp 

import json 
with open("data/classes.json", "w") as f:
    json.dump(label_encoder.classes_, f)
with open("data/max_len.json", "w") as f:
    json.dump(result, MAX_LEN)