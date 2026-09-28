# Documentation

Each version has two records of the same work, written with `dual_track.py`
(IBM style, DITA topics), for two different readers:

| | Plain language: what it means for you | Developer: what changed, why, and how to verify it |
|---|---|---|
| **Windows** 2.5.1-gorilla.1 and gorilla.2 | [windows/gorilla-fancontrol_session_layman.md](windows/gorilla-fancontrol_session_layman.md) | [windows/gorilla-fancontrol_session_developer.md](windows/gorilla-fancontrol_session_developer.md) |
| **Linux** 0.1.0 | [linux/linux_session_layman.md](linux/linux_session_layman.md) | [linux/linux_session_developer.md](linux/linux_session_developer.md) |

The `narrative.txt` in each folder holds the session notes the records were
written from: the decisions, the reasons behind them, what went wrong, and
which tests were run.

Every document ends with a claim-sources table. Each conclusion is marked either as stated in the source material or as an inference, so you can tell a
measured fact from a judgement.
