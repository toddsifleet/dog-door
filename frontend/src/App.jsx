import logo from "./logo.svg";
import styles from "./App.module.css";

import { createSignal } from "solid-js";
import { QuickpingClient } from "quickping";

// const WS_HOST = "ws://localhost:57";
const WS_HOST = "wss://rt.quickping.io";

function App() {
  const [device, setDevice] = createSignal({ state: "CONNECTING" });

  const [deviceUUID, apiKey] = location.pathname.split("/").slice(1);

  function onMessage(message) {
    if (message.method === "P") {
      setDevice({ ...device(), state: message.body });
    }
  }
  const client = new QuickpingClient(
    WS_HOST,
    apiKey,
    async (client) => {
      await client.subscribe(deviceUUID);
    },
    onMessage
  );

  function getButtonText(state) {
    switch (state) {
      case "OPEN":
        return "CLOSE";
      case "CLOSED":
        return "OPEN";
      default:
        return state;
    }
  }
  return (
    <div
      class={styles.button}
      classList={{
        green: device() && device().state === "OPEN",
        red: device() && device().state === "CLOSED",
        grey: device() && device().state === "CONNECTING",
      }}
      onClick={() => {
        client.sendMessage({
          device_uuid: deviceUUID,
          method: "C",
          body: "T",
        });
      }}
    >
      <h1>{device() && getButtonText(device().state)}</h1>
    </div>
  );
}

export default App;
