#ifndef SECRETS_H
#define SECRETS_H
// ============================================================
// OPTIONAL. The display's Setup page (Settings > Device > Setup page,
// or the SmartDisplay-Setup portal) sets all of these without code.
// For development: copy this file to secrets.h (same folder) and fill
// in the values; they are used until something is saved on the Setup page.
// secrets.h is listed in .gitignore - never upload it to GitHub.
//
// Firebase values come from the Firebase console (project smart-display-5f14b):
//   - database URL:  Realtime Database > Data tab, the link at the top
//   - Web API Key:   Project settings > General > Your apps > apiKey
//   - email/password: the display's user, Authentication > Users
// ============================================================

#define FIREBASE_DATABASE_URL  "https://smart-display-5f14b-default-rtdb.firebaseio.com"
#define FIREBASE_API_KEY       "PASTE_WEB_API_KEY_HERE"
#define FIREBASE_USER_EMAIL    "PASTE_DISPLAY_USER_EMAIL_HERE"
#define FIREBASE_USER_PASSWORD "PASTE_DISPLAY_USER_PASSWORD_HERE"

// ============================================================
// Google Calendar (optional). Without these lines, or with the
// PASTE_ values, the calendar shows "Calendar not set up".
//   - Calendar ID: Google Calendar > the calendar's Settings and sharing >
//     Integrate calendar > Calendar ID (the calendar must be public)
//   - API key: console.cloud.google.com, project smart-display-5f14b >
//     APIs & Services > Credentials > Create credentials > API key
//     (Google Calendar API must be enabled in the same project)
// ============================================================

#define CALENDAR_ID            "PASTE_CALENDAR_ID_HERE"
#define CALENDAR_API_KEY       "PASTE_CALENDAR_API_KEY_HERE"

#endif
