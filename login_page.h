#ifndef LOGIN_PAGE_H
#define LOGIN_PAGE_H

static const char LOGIN_HTML[] PROGMEM = R"LOGINPAGE(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Bell — Login</title>
<style>
:root{
  --bg:#14161a; --panel:#1c1f26; --panel2:#232730; --border:#2c3038;
  --text:#e8e9ec; --dim:#9aa0ac; --accent:#7c5cff; --accent2:#6a4ce8; --red:#ff5c5c;
}
*{box-sizing:border-box;}
body{background:var(--bg); color:var(--text); font-family:-apple-system,'Segoe UI',Roboto,sans-serif;
     margin:0; min-height:100vh; display:flex; align-items:center; justify-content:center; padding:16px;}
.card{background:var(--panel); border:1px solid var(--border); border-radius:14px;
      padding:28px 26px; width:100%; max-width:320px;}
.card h1{margin:0 0 4px; font-size:18px;}
.card p.sub{margin:0 0 20px; font-size:13px; color:var(--dim);}
.field{margin-bottom:14px; display:flex; flex-direction:column; gap:5px;}
.field label{font-size:12px; color:var(--dim); font-weight:600;}
input{background:var(--panel2); border:1px solid var(--border); color:var(--text);
      padding:10px 12px; font-size:14px; border-radius:8px; width:100%;}
.btn{width:100%; border:none; padding:11px; border-radius:20px; color:#fff; cursor:pointer;
     font-weight:600; font-size:14px; background:var(--accent); margin-top:6px;}
.btn:hover{background:var(--accent2);}
.error{background:rgba(255,92,92,.1); border:1px solid var(--red); color:var(--red);
       padding:8px 12px; border-radius:8px; font-size:13px; margin-bottom:14px; display:none;}
.hint{margin-top:18px; font-size:12px; color:var(--dim); line-height:1.5; text-align:center;}
</style>
</head><body>
<div class="card">
  <h1>Smart Bell</h1>
  <p class="sub">Nasir Higher Secondary School</p>
  <div id="errorBox" class="error">Incorrect username or password.</div>
  <form method="POST" action="/login">
    <div class="field">
      <label>Username</label>
      <input type="text" name="username" autocomplete="username" autofocus required>
    </div>
    <div class="field">
      <label>Password</label>
      <input type="password" name="password" autocomplete="current-password" required>
    </div>
    <button class="btn" type="submit">Log In</button>
  </form>
  <p class="hint">Forgot password? Hold the reset button on the device for 5 seconds while powering it on to restore the default password.</p>
</div>
<script>
if (location.search.indexOf('error=1') !== -1) {
  document.getElementById('errorBox').style.display = 'block';
}
</script>
</body></html>
)LOGINPAGE";

#endif // LOGIN_PAGE_H
