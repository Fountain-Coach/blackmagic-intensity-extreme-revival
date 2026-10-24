# Linux capture-ingest service. Physical Blackmagic/Thunderbolt control remains on Sierra.
FROM python:3.12-slim-bookworm
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    FOUNTAIN_SCRATCH_INGEST_TOKEN_FILE=/run/secrets/fountain_scratch_token
WORKDIR /opt/fountain-video
COPY simulator/ ./simulator/
USER 65532:65532
EXPOSE 8273
CMD ["python3", "-m", "simulator.scratch_server", "--host", "0.0.0.0", "--port", "8273"]
