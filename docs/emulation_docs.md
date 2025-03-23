# Emulation  

## Summary  
We want to **mock a set of distributed servers** running over an **asynchronous network**. To achieve this, we will create an **Emulation** class that provides the following capabilities:  

- **Spawn new "servers" (processes).**  
- **Send messages** between processes.  
- **Kill and revive processes.**  
- **Introduce network delays and instability.**  
- **Simulate network partitions.**  
- **Ensure proper message queuing and delivery.**  

---

## Documentation  

### **Globals**  

- **`messageQueueMap`** – A map that stores a **message queue** for each process.  
- **`averageMessageDelay`** – A configurable value used to calculate message delay.  
- **`reachableMap`** – A map that tracks **process reachability**, indicating which processes can communicate.  
  - Example: `{1 -> 2, 4; 2 -> 1; 4 -> 1}`  
  - This setup represents a partition where process **4** and process **2** cannot communicate.  
- **`isDead`** – A boolean indicating whether a process is **dead (inactive)**.  

---

### **Methods**  

#### **`spawn(pid, fn)`**  
- Creates a **new process** with the given `pid`.  
- The process starts executing the function `fn`.  

#### **`send(receiverPid, message)`**  
- Sends `message` to `receiverPid`.  
- The message is added to `messageQueueMap` in `receiverPid`'s queue.  

#### **`canSend(receiverPid)`**  
- Checks if the **network allows communication** between the sender and `receiverPid` (i.e., no partition exists).  

#### **`partitionNetwork(groupAPids, groupBPids)`**  
- **Partitions the network**, preventing any process in `groupAPids` from sending messages to a process in `groupBPids`.  
- Updates the `reachableMap` accordingly.  

#### **`killProcess(pid)`**  
- **Temporarily "kills"** the process `pid`.  
- This is done by setting `isDead` to `true` and placing the process in a loop that checks if `reviveProcess` has reset `isDead`.  

#### **`reviveProcess(pid)`**  
- **Revives** a previously killed process by setting `isDead` to `false`.  

#### **`connectNetwork(groupAPids, groupBPids)`**  
- **Reconnects processes** previously partitioned by `partitionNetwork`.  
- Updates `reachableMap` to allow message passing between `groupAPids` and `groupBPids`.  

#### **`receiveMessage()`**  
- Retrieves the **next message** from the process's **message queue**.  

---
