"""Calendar for the Desk Display: upcoming items from a Notion database.

Needs two things in settings.json:
    "notion_token"        the secret of a Notion integration you create (starts with ntn_)
    "notion_database_id"  the database to read; it must be shared with that integration

It reads only. The first date column is used as the date (or name one with
"notion_date_property"). Rows ticked off with a Done checkbox or a Done status are skipped.
"""
from __future__ import annotations

import datetime
import json
import re
import urllib.error
import urllib.request
from typing import Optional

API = "https://api.notion.com/v1"
API_VERSION = "2025-09-03"
DAYS_AHEAD = 7
MAX_SENT = 10
DONE_NAMES = {"done", "complete", "completed", "finished", "canceled", "cancelled"}


class CalendarError(Exception):
    """Carries a short message that fits on the display."""


def _call(token: str, method: str, path: str, body: Optional[dict] = None, timeout: float = 12.0):
    req = urllib.request.Request(
        API + path, method=method,
        data=json.dumps(body).encode() if body is not None else None,
        headers={"Authorization": f"Bearer {token}", "Notion-Version": API_VERSION, "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)


def clean_id(text: str) -> str:
    """Accepts a bare id, an id with dashes, or a whole Notion link."""
    found = re.findall(r"[0-9a-fA-F]{32}", text.replace("-", ""))
    if not found:
        raise CalendarError("The Notion database id looks wrong")
    return found[0].lower()


def find_data_source(token: str, database_id: str, call=_call) -> dict:
    """A Notion database holds one or more data sources; rows are read from a data source."""
    ident = clean_id(database_id)
    try:
        sources = call(token, "GET", f"/databases/{ident}").get("data_sources") or []
        if sources:
            ident = sources[0]["id"]
    except urllib.error.HTTPError as e:
        if e.code == 401:
            raise
    return call(token, "GET", f"/data_sources/{ident}")


def pick_columns(source: dict, date_override: str = ""):
    """-> (title column, date column, [checkbox columns that mean done], [status columns])"""
    props = source.get("properties", {})
    title = next((n for n, p in props.items() if p.get("type") == "title"), None)
    dates = [n for n, p in props.items() if p.get("type") == "date"]
    if date_override:
        date = date_override if date_override in dates else None
    else:
        preferred = ["due date", "due", "date", "starts", "start", "when"]
        date = next((n for want in preferred for n in dates if n.lower() == want), dates[0] if dates else None)
    if not title or not date:
        raise CalendarError("That Notion database has no date column")
    checks = [n for n, p in props.items() if p.get("type") == "checkbox" and n.lower() in DONE_NAMES]
    statuses = [n for n, p in props.items() if p.get("type") == "status"]
    return title, date, checks, statuses


def _parse_when(text: str):
    """Notion date text -> (local date or datetime, is all day)"""
    if "T" not in text:
        return datetime.date.fromisoformat(text[:10]), True
    dt = datetime.datetime.fromisoformat(text.replace("Z", "+00:00"))
    if dt.tzinfo is not None:
        dt = dt.astimezone().replace(tzinfo=None)
    return dt, False


def read_events(token: str, source_id: str, columns, now: Optional[datetime.datetime] = None, call=_call):
    """-> list of {"date", "start" (datetime, or None for all day), "end", "title"}, soonest first."""
    now = now or datetime.datetime.now()
    title_col, date_col, checks, statuses = columns
    first = (now.date() - datetime.timedelta(days=1)).isoformat()
    last = (now.date() + datetime.timedelta(days=DAYS_AHEAD + 1)).isoformat()
    body = {
        "filter": {"and": [{"property": date_col, "date": {"on_or_after": first}},
                           {"property": date_col, "date": {"on_or_before": last}}]},
        "sorts": [{"property": date_col, "direction": "ascending"}],
        "page_size": 100,
    }
    events, cursor = [], None
    for _ in range(3):
        if cursor:
            body["start_cursor"] = cursor
        page = call(token, "POST", f"/data_sources/{source_id}/query", body)
        for row in page.get("results", []):
            props = row.get("properties", {})
            if any((props.get(c) or {}).get("checkbox") for c in checks):
                continue
            if any((((props.get(s) or {}).get("status") or {}).get("name") or "").lower() in DONE_NAMES for s in statuses):
                continue
            date = (props.get(date_col) or {}).get("date")
            if not date or not date.get("start"):
                continue
            title = "".join(t.get("plain_text", "") for t in (props.get(title_col) or {}).get("title", [])).strip()
            if not title:
                continue
            try:
                start, all_day = _parse_when(date["start"])
                end = _parse_when(date["end"])[0] if date.get("end") else None
            except ValueError:
                continue
            if all_day:
                if not (now.date() <= start <= now.date() + datetime.timedelta(days=DAYS_AHEAD)):
                    continue
                events.append({"date": start, "start": None, "end": None, "title": title})
            else:
                finish = end if isinstance(end, datetime.datetime) else start + datetime.timedelta(hours=1)
                if finish < now or start.date() > now.date() + datetime.timedelta(days=DAYS_AHEAD):
                    continue
                events.append({"date": start.date(), "start": start, "end": finish, "title": title})
        cursor = page.get("next_cursor")
        if not page.get("has_more") or not cursor:
            break
    events.sort(key=lambda e: (e["date"], e["start"] is not None, e["start"] or datetime.datetime.min, e["title"]))
    return events


def day_label(day: datetime.date, today: datetime.date) -> str:
    if day == today:
        return "TODAY"
    if day == today + datetime.timedelta(days=1):
        return "TOMORROW"
    return f"{day:%a}".upper() + f" {day.day}"


def events_payload(events, h24: bool, fmt_clock, to_ascii, now: Optional[datetime.datetime] = None) -> bytes:
    now = now or datetime.datetime.now()
    lines = [b"OK"]
    for e in events:
        start = e["start"]
        if start is not None and (e.get("end") or start + datetime.timedelta(hours=1)) < now:
            continue                                   # it has ended since the calendar was last read
        if e["date"] < now.date():
            continue
        when = fmt_clock(start.hour, start.minute, h24) if start is not None else "ALL DAY"
        lines.append(day_label(e["date"], now.date()).encode() + b"|" + when.encode() + b"|" + to_ascii(e["title"], 64).replace(b"|", b"/"))
        if len(lines) > MAX_SENT:
            break
    return b"\n".join(lines) + b"\n"


def error_payload(message: str) -> bytes:
    return b"ERR\n" + message.encode("ascii", "replace")[:60] + b"\n"


def explain(error: Exception) -> str:
    """Turns a failure into a line short enough for the display."""
    if isinstance(error, CalendarError):
        return str(error)
    if isinstance(error, urllib.error.HTTPError):
        if error.code == 401:
            return "Notion did not accept the token"
        if error.code in (400, 403, 404):
            return "Notion can't see that database yet"
        if error.code == 429:
            return "Notion is busy. Trying again soon."
        return f"Notion answered with error {error.code}"
    return "Could not reach Notion"


def demo_events(now: Optional[datetime.datetime] = None):
    now = now or datetime.datetime.now()
    today = now.date()
    def at(days, hour, minute):
        return datetime.datetime.combine(today + datetime.timedelta(days=days), datetime.time(hour, minute))
    soon = (now + datetime.timedelta(minutes=8)).replace(second=0, microsecond=0)
    return [
        {"date": today, "start": None, "title": "Demo: all-day item"},
        {"date": soon.date(), "start": soon, "title": "Demo: meeting that starts soon"},
        {"date": today + datetime.timedelta(days=1), "start": at(1, 14, 0), "title": "Demo: afternoon appointment"},
        {"date": today + datetime.timedelta(days=2), "start": None, "title": "Demo: deadline"},
        {"date": today + datetime.timedelta(days=2), "start": at(2, 11, 15), "title": "Demo: coffee"},
        {"date": today + datetime.timedelta(days=4), "start": at(4, 18, 0), "title": "Demo: dinner with a long name that will not fit on one line"},
    ]
