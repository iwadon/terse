#!/usr/bin/env python3
# 環境変数:
# APP_ID, APP_PRIVATE_KEY, (任意) INSTALLATION_ID
import os, time, sys, requests, jwt

APP_ID = os.environ.get("APP_ID")
PRIVATE_KEY = os.environ.get("APP_PRIVATE_KEY")
INSTALLATION_ID = os.environ.get("INSTALLATION_ID")

if not APP_ID or not PRIVATE_KEY:
    print("Missing APP_ID or APP_PRIVATE_KEY env vars", file=sys.stderr)
    sys.exit(1)

# JWT 作成
now = int(time.time())
payload = {
    "iat": now - 60,
    "exp": now + (9 * 60),  # 十分短く
    "iss": APP_ID
}

# PyJWT を使って署名（RS256）
jwt_token = jwt.encode(payload, PRIVATE_KEY, algorithm="RS256")

headers = {
    "Authorization": f"Bearer {jwt_token}",
    "Accept": "application/vnd.github+json"
}

try:
    if INSTALLATION_ID:
        url = f"https://api.github.com/app/installations/{INSTALLATION_ID}/access_tokens"
        r = requests.post(url, headers=headers)
    else:
        # installation を取得して最初のものを使う（個人アカウントなら通常1つ）
        r = requests.get("https://api.github.com/app/installations", headers=headers)
        r.raise_for_status()
        items = r.json()
        if not items:
            print("No installations found for the app", file=sys.stderr)
            sys.exit(1)
        inst_id = items[0]["id"]
        url = f"https://api.github.com/app/installations/{inst_id}/access_tokens"
        r = requests.post(url, headers=headers)

    r.raise_for_status()
    token = r.json().get("token")
    if not token:
        print("No token in response", file=sys.stderr)
        sys.exit(1)
    print(token)
except Exception as e:
    print("Error obtaining installation token:", e, file=sys.stderr)
    sys.exit(1)
