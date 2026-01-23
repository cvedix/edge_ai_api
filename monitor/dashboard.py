import pandas as pd
import streamlit as st
from pathlib import Path

# ================= PAGE CONFIG =================
st.set_page_config(
    page_title="CVEDIX Instance Monitor Dashboard",
    layout="wide",
)

st.title("📊 CVEDIX Instance Monitor Dashboard")

# ================= DEFAULT CSV =================
DEFAULT_CSV = Path("instance_monitor.csv")

# ================= FILE UPLOADER =================
uploaded_file = st.file_uploader(
    "📂 Upload instance_monitor CSV (optional)",
    type=["csv"]
)

# ================= LOAD DATA =================
if uploaded_file is not None:
    df = pd.read_csv(uploaded_file)
    data_source = f"Uploaded file: {uploaded_file.name}"

elif DEFAULT_CSV.exists():
    df = pd.read_csv(DEFAULT_CSV)
    data_source = f"Auto-loaded file: {DEFAULT_CSV.name}"

else:
    st.error("❌ CSV not found. Please upload a file.")
    st.stop()

# ================= PREPROCESS =================
df["timestamp"] = pd.to_datetime(df["timestamp"])

st.caption(f"📄 Data source: {data_source}")

# ================= FILTER =================
instance_ids = df["instance_id"].unique()
selected_instance = st.selectbox(
    "🎯 Select Instance",
    instance_ids
)

df_i = df[df["instance_id"] == selected_instance]

# ================= METRICS =================
c1, c2, c3, c4 = st.columns(4)

c1.metric("FPS (list)", f"{df_i['fps'].iloc[-1]:.2f}")
c2.metric("FPS (runtime)", f"{df_i['current_framerate'].iloc[-1]:.2f}")
c3.metric("Latency (ms)", int(df_i["latency"].iloc[-1]))
c4.metric("CPU (%)", f"{df_i['cpu_usage_percent'].iloc[-1]:.1f}")

# ================= CHARTS =================
st.subheader("📈 Performance Over Time")

st.line_chart(
    df_i.set_index("timestamp")[["fps", "current_framerate"]],
    height=300,
)

st.line_chart(
    df_i.set_index("timestamp")[["latency"]],
    height=300,
)

st.line_chart(
    df_i.set_index("timestamp")[["dropped_frames_count"]],
    height=300,
)

# ================= TABLE =================
st.subheader("📄 Raw Data")
st.dataframe(df_i, use_container_width=True)
