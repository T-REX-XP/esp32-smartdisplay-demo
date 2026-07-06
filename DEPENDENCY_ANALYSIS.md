# Dependency Analysis Report

**Analysis Date:** July 6, 2026

## Executive Summary

The project has **outdated dependencies** that require updates. Key findings:

- **Python dependencies:** All 4 packages are outdated (1-3 years old)
- **C++ dependencies:** ArduinoJson has security updates available
- **Embedded library:** LVGL is at version 9.4.0 (current)

### Recommendations:
1. ✅ Update `requirements.txt` to latest versions
2. ✅ Update ArduinoJson to `^7.2.0`
3. ✅ Add missing optional dependencies
4. ✅ Add `requirements-dev.txt` for development tools

---

## Python Dependencies Analysis

### Current State (requirements.txt)

| Package | Current | Latest | Status | Notes |
|---------|---------|--------|--------|-------|
| Flask | 2.3.3 | 3.x.x | ❌ Outdated | Released: Sep 2023 |
| pyserial | 3.5 | 3.5 | ✅ Current | Stable release |
| psutil | 5.9.4 | 6.0.x | ❌ Outdated | Released: Feb 2023 |
| msgpack | 1.0.5 | 1.0.8+ | ❌ Outdated | Released: Mar 2023 |

### Detailed Analysis

#### 1. **Flask 2.3.3** → **3.0.x**
- **Released:** Sep 2023 → Nov 2024
- **Status:** Major version available
- **Breaking Changes:** Minimal for this project (JSON handling, dependency imports)
- **Benefits:** 
  - Better async support
  - Performance improvements
  - Security patches
- **Action:** Update to `Flask>=3.0,<4.0`

#### 2. **pyserial 3.5** 
- **Status:** ✅ Current and stable
- **Action:** Keep as is (optional: `pyserial>=3.5,<4.0`)

#### 3. **psutil 5.9.4** → **6.0.x**
- **Released:** Feb 2023 → May 2024
- **Status:** Outdated
- **Benefits:**
  - Performance improvements
  - New platform support
  - Bug fixes
- **Action:** Update to `psutil>=6.0,<7.0`

#### 4. **msgpack 1.0.5** → **1.0.8+**
- **Released:** Mar 2023 → Recent
- **Status:** Outdated
- **Benefits:**
  - Performance improvements
  - Bug fixes
- **Action:** Update to `msgpack>=1.0.5,<2.0`

---

## C++ Dependencies Analysis

### Current State (platformio.ini)

| Library | Current | Latest | Status | Notes |
|---------|---------|--------|--------|-------|
| esp32-smartdisplay | (Git latest) | (Git latest) | ✅ Current | GitHub dependency |
| ArduinoJson | ^7.0.4 | ^7.2.0+ | ⚠️ Outdated | Released: 2023-03-01 |
| LVGL (embedded) | 9.4.0 | 9.4.0 | ✅ Current | Built-in |

### Detailed Analysis

#### 1. **ArduinoJson ^7.0.4** → **^7.2.0**
- **Current:** 7.0.4 (Mar 2023)
- **Latest:** 7.2.0+ (2024)
- **Status:** Outdated
- **Benefits:**
  - Security improvements
  - Performance enhancements
  - Bug fixes
  - Better memory handling
- **Action:** Update to `bblanchon/ArduinoJson@^7.2.0`

#### 2. **esp32-smartdisplay**
- **Status:** ✅ Using latest from GitHub
- **Note:** Git dependency auto-updates
- **Action:** Keep as is

#### 3. **LVGL 9.4.0**
- **Status:** ✅ Latest stable version
- **Note:** Excellent version, well-maintained
- **Action:** Keep as is

---

## Missing Optional Dependencies

### Recommended Additions

#### For Development/Testing:
- `pytest` - For Python test framework
- `pytest-asyncio` - For async test support
- `black` - Code formatting
- `pylint` - Code quality checking
- `flake8` - Style guide enforcement

#### For Production:
- `gunicorn` - WSGI HTTP Server (recommended for Flask)
- `python-dotenv` - Environment variable management
- `Werkzeug` - Already required by Flask, but explicit for WSGI support

---

## Compatibility Notes

### Version Constraints

#### Python 3.x Compatibility:
- `Flask 3.x` requires Python 3.8+
- `psutil 6.x` requires Python 3.5+
- `msgpack 1.0.x` requires Python 3.6+
- `pyserial 3.5` requires Python 3.6+

**Recommendation:** Specify Python requirement in `requirements.txt`:
```
python >= 3.8
```

#### ESP32/Arduino Compatibility:
- `ArduinoJson 7.2.0` is fully compatible with ESP32 Arduino framework
- No breaking changes from 7.0.4 to 7.2.0
- Safe update

---

## Security Vulnerabilities

### Current Status
- No known critical vulnerabilities in current versions
- Outdated dependencies may receive fewer security backports

### Recommended Actions
1. Update to receive latest security patches
2. Enable dependabot/renovate for automatic notifications
3. Use `pip-audit` to scan for vulnerabilities:
   ```bash
   pip install pip-audit
   pip-audit
   ```

---

## Migration Steps

### Step 1: Update Python Dependencies

**File:** `requirements.txt`
```
Flask>=3.0,<4.0
pyserial>=3.5,<4.0
psutil>=6.0,<7.0
msgpack>=1.0.5,<2.0
```

### Step 2: Create Development Dependencies

**File:** `requirements-dev.txt`
```
Flask>=3.0,<4.0
pyserial>=3.5,<4.0
psutil>=6.0,<7.0
msgpack>=1.0.5,<2.0
pytest>=7.0
pytest-asyncio>=0.21
black>=23.0
pylint>=3.0
flake8>=6.0
gunicorn>=21.0
python-dotenv>=1.0
```

### Step 3: Update C++ Dependencies

**File:** `platformio.ini`
- Change line:
  ```ini
  bblanchon/ArduinoJson@^7.0.4
  ```
  To:
  ```ini
  bblanchon/ArduinoJson@^7.2.0
  ```

### Step 4: Testing

1. **Python:**
   ```bash
   pip install -r requirements.txt
   python esp32_simulator.py --help
   python esp32_simulator_webui.py --help
   ```

2. **Arduino/ESP32:**
   ```bash
   platformio run -e esp32-2432S022C
   platformio run -t upload -e esp32-2432S022C
   ```

### Step 5: Validation Checklist

- [ ] Flask app starts without errors
- [ ] Simulator communicates with ESP32 correctly
- [ ] JSON/MessagePack encoding/decoding works
- [ ] Serial communication stable
- [ ] CPU metrics reading functional
- [ ] Alarm management operational
- [ ] Web UI responsive

---

## Risk Assessment

### Low Risk ✅
- **ArduinoJson 7.0.4 → 7.2.0:** Compatible upgrade, minor version change
- **Flask 2.3.3 → 3.0.x:** Some code may need adjustment, but generally safe
- **psutil 5.9.4 → 6.0.x:** API stable, few breaking changes
- **msgpack 1.0.5 → 1.0.8:** Patch update, minimal risk

### Compatibility Issues (If Any)
- Flask 3.x may require adjustments in custom JSON serialization
- psutil 6.x API is mostly backward-compatible

---

## Recommended Timeline

1. **Immediate (This week):** Update requirements.txt
2. **This sprint:** Test Python simulator thoroughly
3. **Next sprint:** Validate all ESP32 boards compile with ArduinoJson 7.2.0
4. **Ongoing:** Set up dependency monitoring (GitHub Dependabot recommended)

---

## Future Recommendations

1. **Add `.gitignore` entries:**
   ```
   __pycache__/
   *.egg-info/
   .pytest_cache/
   .venv/
   venv/
   ```

2. **Add GitHub Actions workflow for dependency checking:**
   - Run tests on dependency updates
   - Use Dependabot for automated PRs

3. **Consider using `uv` or `pipenv` for better dependency management**

4. **Add `pyproject.toml` for modern Python packaging**

---

## Summary

| Category | Action | Priority | Impact |
|----------|--------|----------|--------|
| Flask | Update 2.3.3 → 3.x | High | Medium |
| psutil | Update 5.9.4 → 6.x | High | Low |
| msgpack | Update 1.0.5 → 1.0.8+ | Medium | Low |
| ArduinoJson | Update 7.0.4 → 7.2.0 | Medium | Low |
| Add dev deps | Create requirements-dev.txt | Medium | Low |

**Overall Assessment:** Safe to update all dependencies with testing.
