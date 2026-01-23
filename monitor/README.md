# CVEDIX Instance Monitor Dashboard

A lightweight monitoring dashboard for visualizing CVEDIX Edge AI instance metrics from CSV files.

The dashboard reads monitoring data (FPS, latency, dropped frames, CPU/RAM usage, etc.) and renders
interactive charts and tables in a web UI using **Python + Streamlit**.

---

## 1. Features

- Auto-load monitoring data from a default CSV file
- Optional CSV upload to override default data
- Filter and view metrics by instance ID
- Time-series charts:
  - FPS (instance list vs runtime statistics)
  - Latency
  - Dropped frames
- Key metrics cards (FPS, latency, CPU)
- View raw CSV data in tabular format

---

## 2. Project Structure

.
├── dashboard.py
├── instance_monitor.csv
├── requirements.txt
└── README.md

---

## 3. CSV Format

Required columns:

timestamp  
instance_id  
display_name  
solution  
running  
fps  
cpu_usage_percent  
ram_used_mb  
ram_total_mb  
current_framerate  
dropped_frames_count  
frames_processed  
latency  
resolution  
source_framerate  

---

## 4. Requirements

- Python 3.8 or newer
- pip

---

## 5. Installing Dependencies

### Option A – User-level install (recommended)

python -m pip install --user pandas streamlit

If streamlit command is not found:

export PATH=$HOME/.local/bin:$PATH

---

### Option B – Virtual Environment

python -m venv venv  
source venv/bin/activate  
pip install pandas streamlit  

---

## 6. Running the Dashboard

python -m streamlit run dashboard.py

Open browser at:

http://localhost:8501

---

## 7. Auto-load Behavior

- instance_monitor.csv is automatically loaded if present
- Uploaded CSV overrides default file
- If no CSV exists, user will be prompted to upload

---

## 8. Remote Server Access

ssh -L 8501:localhost:8501 user@server-ip

Then open:

http://localhost:8501

---

## 9. Notes

- fps and current_framerate may differ
- File-based sources may drop FPS to 0 after completion
- High latency + dropped frames indicate overload

---

## 10. License

Internal use / demo / evaluation.
