"""Interactively send requests to the relay HTTPS API."""

import json
import ssl
import threading
import tkinter as tk
from tkinter import ttk
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from api_tester import format_body


PROTOCOLS = ("SRT", "SRTLA", "RTSP", "RTMP", "WebRTC", "MoQ", "HLS", "MPEGTS", "RTP")


def send_request(method, url, key, protocol, stream_key):
	context = ssl._create_unverified_context()
	headers = {"Authorization": key}
	data = None

	if method in ("POST", "DELETE"):
		data = json.dumps({"protocol": protocol, "streamKey": stream_key}).encode("utf-8")
		headers["Content-Type"] = "application/json"

	try:
		request = Request(url, data=data, method=method, headers=headers)
		with urlopen(request, context=context, timeout=5) as response:
			return response.status, response.read().decode("utf-8", errors="replace"), None
	except HTTPError as error:
		return error.code, error.read().decode("utf-8", errors="replace"), None
	except (URLError, TimeoutError, OSError, ValueError) as error:
		return None, "", str(error.reason if isinstance(error, URLError) and error.reason else error)


class RelayApiTester(tk.Tk):
	def __init__(self):
		super().__init__()
		self.title("Relay API tester")
		self.geometry("760x480")
		self.minsize(620, 400)

		self.host = tk.StringVar(value="127.0.0.1")
		self.port = tk.StringVar(value="8080")
		self.access_key = tk.StringVar()
		self.protocol = tk.StringVar(value="SRT")
		self.stream_key = tk.StringVar()
		self.status = tk.StringVar(value="Idle")
		self._build()

	def _build(self):
		connection = ttk.LabelFrame(self, text="Connection", padding=8)
		connection.pack(fill="x", padx=8, pady=(8, 4))
		ttk.Label(connection, text="Host").grid(row=0, column=0, sticky="w")
		ttk.Label(connection, text="Port").grid(row=0, column=1, sticky="w")
		ttk.Label(connection, text="API key").grid(row=0, column=2, sticky="w")
		ttk.Entry(connection, textvariable=self.host, width=18).grid(row=1, column=0, sticky="we", padx=(0, 8))
		ttk.Entry(connection, textvariable=self.port, width=8).grid(row=1, column=1, sticky="we", padx=(0, 8))
		ttk.Entry(connection, textvariable=self.access_key, show="*").grid(row=1, column=2, sticky="we")
		connection.columnconfigure(2, weight=1)

		feed = ttk.LabelFrame(self, text="Feed", padding=8)
		feed.pack(fill="x", padx=8, pady=4)
		ttk.Label(feed, text="Protocol").grid(row=0, column=0, sticky="w")
		ttk.Label(feed, text="Stream key").grid(row=0, column=1, sticky="w")
		ttk.Combobox(feed, textvariable=self.protocol, values=PROTOCOLS, state="readonly", width=12).grid(
			row=1, column=0, sticky="we", padx=(0, 8)
		)
		ttk.Entry(feed, textvariable=self.stream_key).grid(row=1, column=1, sticky="we")
		feed.columnconfigure(1, weight=1)

		buttons = ttk.LabelFrame(self, text="Endpoints", padding=8)
		buttons.pack(fill="x", padx=8, pady=4)
		for method, label in (("POST", "Create feed"), ("DELETE", "Delete feed"), ("GET", "List feeds")):
			ttk.Button(buttons, text=label, command=lambda selected=method: self.send(selected)).pack(
				side="left", padx=(0, 8)
			)

		ttk.Label(self, textvariable=self.status).pack(anchor="w", padx=12, pady=(4, 0))
		output = ttk.LabelFrame(self, text="Response", padding=8)
		output.pack(fill="both", expand=True, padx=8, pady=(4, 8))
		self.response = tk.Text(output, wrap="word")
		scroll = ttk.Scrollbar(output, command=self.response.yview)
		self.response.configure(yscrollcommand=scroll.set)
		self.response.pack(side="left", fill="both", expand=True)
		scroll.pack(side="right", fill="y")

	def send(self, method):
		host = self.host.get().strip()
		port = self.port.get().strip()
		url = f"https://{host}:{port}/feeds"
		args = (method, url, self.access_key.get(), self.protocol.get(), self.stream_key.get())
		self.status.set(f"Sending {method} {url}")
		threading.Thread(target=self._request, args=args, daemon=True).start()

	def _request(self, method, url, key, protocol, stream_key):
		status, body, error = send_request(method, url, key, protocol, stream_key)
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


if __name__ == "__main__":
	RelayApiTester().mainloop()
