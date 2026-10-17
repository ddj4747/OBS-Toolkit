"""Send test requests to the plugin HTTPS API.

The server uses a self-signed certificate, so certificate checks are disabled.
The access key is sent as the Authorization header value.
"""

import json
import ssl
import threading
import tkinter as tk
from tkinter import ttk
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qsl, quote, urlencode
from urllib.request import Request, urlopen

ENDPOINTS = (
	("GET", "/getBitrates"),
	("GET", "/getSceneList"),
	("GET", "/getPluginSources"),
	("GET", "/streamState"),
	("POST", "/startStream"),
	("POST", "/stopStream"),
	("POST", "/setActiveScene"),
)

PAYLOADS = (
	"",
	"sceneName=Scene",
	"sceneName=Scene 2",
)


def build_query(path, payload):
	if payload == "":
		return ""
	if path == "/setActiveScene" and "=" not in payload:
		pairs = [("sceneName", payload)]
	else:
		pairs = parse_qsl(payload, keep_blank_values=True)
		if not pairs:
			return quote(payload, safe="=&")
	return urlencode(pairs, quote_via=quote)


def format_body(body):
	if not body:
		return "(empty)"
	try:
		return json.dumps(json.loads(body), indent=2)
	except json.JSONDecodeError:
		return body


class ApiTester(tk.Tk):
	def __init__(self):
		super().__init__()
		self.title("Plugin API tester")
		self.geometry("760x560")
		self.minsize(640, 480)

		self.host = tk.StringVar(value="127.0.0.1")
		self.port = tk.StringVar(value="5643")
		self.access_key = tk.StringVar(value="test123")
		self.status = tk.StringVar(value="Idle")

		self._build()

	def _build(self):
		connection = ttk.LabelFrame(self, text="Connection", padding=8)
		connection.pack(fill="x", padx=8, pady=(8, 4))

		ttk.Label(connection, text="Host").grid(row=0, column=0, sticky="w")
		ttk.Entry(connection, textvariable=self.host, width=18).grid(row=1, column=0, sticky="we", padx=(0, 8))
		ttk.Label(connection, text="Port").grid(row=0, column=1, sticky="w")
		ttk.Entry(connection, textvariable=self.port, width=8).grid(row=1, column=1, sticky="w", padx=(0, 8))
		ttk.Label(connection, text="Access key").grid(row=0, column=2, sticky="w")
		ttk.Entry(connection, textvariable=self.access_key).grid(row=1, column=2, sticky="we")
		connection.columnconfigure(2, weight=1)

		payload = ttk.LabelFrame(self, text="Payload (query string)", padding=8)
		payload.pack(fill="x", padx=8, pady=4)
		presets = ttk.Frame(payload)
		presets.pack(fill="x")
		ttk.Label(presets, text="Preset").pack(side="left")
		preset = ttk.Combobox(presets, values=PAYLOADS, state="readonly", width=28)
		preset.pack(side="left", padx=(6, 0))
		preset.bind("<<ComboboxSelected>>", lambda _event: self._apply_preset(preset.get()))
		self.payload = tk.Text(payload, height=3, wrap="word")
		self.payload.pack(fill="x", pady=(6, 0))
		self.payload.insert("1.0", PAYLOADS[1])
		ttk.Label(
			payload,
			text="Edit this freely. Spaces and other reserved characters are encoded. "
			"For Set active scene, a value without '=' is sent as sceneName.",
		).pack(anchor="w", pady=(4, 0))

		buttons = ttk.LabelFrame(self, text="Endpoints", padding=8)
		buttons.pack(fill="x", padx=8, pady=4)
		for index, (method, path) in enumerate(ENDPOINTS):
			label = f"{method}  {path}"
			button = ttk.Button(buttons, text=label, command=lambda m=method, p=path: self.send(m, p))
			button.grid(row=index // 2, column=index % 2, sticky="we", padx=4, pady=3)
		buttons.columnconfigure(0, weight=1)
		buttons.columnconfigure(1, weight=1)

		ttk.Label(self, textvariable=self.status, anchor="w").pack(fill="x", padx=12, pady=(4, 0))

		output = ttk.LabelFrame(self, text="Response", padding=8)
		output.pack(fill="both", expand=True, padx=8, pady=(4, 8))
		self.response = tk.Text(output, wrap="word", height=16)
		scroll = ttk.Scrollbar(output, command=self.response.yview)
		self.response.configure(yscrollcommand=scroll.set)
		self.response.pack(side="left", fill="both", expand=True)
		scroll.pack(side="right", fill="y")

	def send(self, method, path):
		host = self.host.get().strip()
		port = self.port.get().strip()
		key = self.access_key.get()
		query = build_query(path, self.payload.get("1.0", "end-1c"))
		url = f"https://{host}:{port}{path}"
		if query:
			url = f"{url}?{query}"

		self.status.set(f"Sending {method} {url}")
		thread = threading.Thread(target=self._request, args=(method, url, key), daemon=True)
		thread.start()

	def _request(self, method, url, key):
		context = ssl._create_unverified_context()
		request = Request(url, method=method, headers={"Authorization": key})
		try:
			with urlopen(request, context=context, timeout=5) as response:
				body = response.read().decode("utf-8", errors="replace")
				status = response.status
				error = None
		except HTTPError as caught:
			status = caught.code
			body = caught.read().decode("utf-8", errors="replace")
			error = None
		except (URLError, TimeoutError, OSError, ValueError) as caught:
			status = None
			body = ""
			error = str(caught.reason if isinstance(caught, URLError) and caught.reason else caught)

		self.after(0, self._show_result, method, url, status, body, error)

	def _show_result(self, method, url, status, body, error):
		if error:
			text = f"{method} {url}\n\nFailed: {error}"
			self.status.set("Request failed")
		else:
			text = f"{method} {url}\nStatus: {status}\n\n{format_body(body)}"
			self.status.set(f"Last status: {status}")

		self.response.delete("1.0", "end")
		self.response.insert("1.0", text)

	def _apply_preset(self, value):
		self.payload.delete("1.0", "end")
		self.payload.insert("1.0", value)


if __name__ == "__main__":
	ApiTester().mainloop()
