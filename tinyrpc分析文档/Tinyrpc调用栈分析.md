## 一、准备
### 1.1 InitConfig
![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744341227711-accd6905-07a1-496e-b061-eaf75c4555d3.png)

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744341280093-54c6dc4f-8f2d-4b21-b39a-ed3464b0d362.png)

解析配置xml文件

1. 解析日志配置。
2. 解析协程配置。
3. 解析其他配置：时间轮、 服务端配置等等。

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744341296533-87b006f0-7e56-4072-a2e0-7a5c2dad9420.png)

创建TcpServer

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744341347114-f8bb0019-7349-4192-9939-6b62169e4967.png)

### 1.2 注册协议类型
根据协议类型注册到TcpServer中

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744341392546-f502fe89-0d61-40f1-a7f8-b4bfe9825649.png)



![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744341432831-3e2e6ce1-8ed6-404d-88e5-18bbe33fc37f.png)



## 二、开启服务


![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744341543853-9c058113-e40b-4cef-887d-fd44ce6fb95f.png)



### 2.1 开启日志服务
注册定时器

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744703095489-5c499133-dbce-4741-a69e-b20df9e43199.png)

时间到了调用回调刷新日志

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744703131548-e225b18d-ff5b-42a3-98b5-b0c73dd7c77d.png)

条件变量通知线程进行刷新

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744703153196-b97741fa-ffeb-4d1a-815d-c26a68dbd291.png)

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744703170063-40efafd6-f03b-423a-b4a4-a983b2616736.png)



### 2.2 开启server服务
+ 创建listen_sock，初始化
+ 创建accept_协程，设置回调
+ 唤醒accept协程
+ 开启线程池，开启MainReactor

### ![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744703378789-bdc727f0-caa1-4bbf-b6af-28fa34cd37f1.png)


#### 2.2.1 accept唤醒协程
+ 获取client_fd
+ 将新的链接交给SubReactor

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744703686095-60b7774f-0f2d-4a60-8b3c-860964e5d41c.png)



![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744704736960-bb2ef9a8-46c8-4d67-ae47-04050ab7240e.png)



![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744703756796-1fdcc827-3408-4d51-b969-7b862af15900.png)

+ 封装fd注册到epoll
+ yield当前协程
+ 等待被resume后进行read

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744703867670-cdff4fd1-4173-45a9-a413-c53ae574aeab.png)  


+ 注册时间轮
+ 注册SubReactor回调

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744704817688-12147830-3875-4dbb-a677-2bfc2905e470.png)

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744704863196-05551ae6-4be6-41a9-834a-a63c26bac668.png)

#### 2.2.2 开启线程池
+ 唤醒线程池中的线程执行任务

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744704110074-07ee7c5a-e62f-478e-915a-1d07ffc4b00e.png)

+ 在信号量下等待被唤醒
+ 执行subReactor开启循环

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744704145109-d594f4e7-f204-4fc2-af32-09c1a4fd2363.png)

#### 2.2.3 开启MainReactor
```plain
void Reactor::loop() {

  assert(isLoopThread());
  if (m_is_looping) {
    // DebugLog << "this reactor is looping!";
    return;
  }
  
  m_is_looping = true;
	m_stop_flag = false;

  Coroutine* first_coroutine = nullptr;

	while(!m_stop_flag) {
		const int MAX_EVENTS = 10;
		epoll_event re_events[MAX_EVENTS + 1];

    if (first_coroutine) {
      tinyrpc::Coroutine::Resume(first_coroutine);
      first_coroutine = NULL;
    }

    // main reactor need't to resume coroutine in global CoroutineTaskQueue, only io thread do this work
    if (m_reactor_type != MainReactor) {
      FdEvent* ptr = NULL;
      // ptr->setReactor(NULL);
      while(1) {
        ptr = CoroutineTaskQueue::GetCoroutineTaskQueue()->pop();
        if (ptr) {
          ptr->setReactor(this);
          tinyrpc::Coroutine::Resume(ptr->getCoroutine()); 
        } else {
          break;
        }
      }
    }


		// DebugLog << "task";
		// excute tasks
    Mutex::Lock lock(m_mutex);
    std::vector<std::function<void()>> tmp_tasks;
    tmp_tasks.swap(m_pending_tasks);
    lock.unlock();

		for (size_t i = 0; i < tmp_tasks.size(); ++i) {
			// DebugLog << "begin to excute task[" << i << "]";
			if (tmp_tasks[i]) {
				tmp_tasks[i]();
			}
			// DebugLog << "end excute tasks[" << i << "]";
		}
		// DebugLog << "to epoll_wait";
		int rt = epoll_wait(m_epfd, re_events, MAX_EVENTS, t_max_epoll_timeout);

		// DebugLog << "epoll_wait back";

		if (rt < 0) {
			ErrorLog << "epoll_wait error, skip, errno=" << strerror(errno);
		} else {
			// DebugLog << "epoll_wait back, rt = " << rt;
			for (int i = 0; i < rt; ++i) {
				epoll_event one_event = re_events[i];	

				if (one_event.data.fd == m_wake_fd && (one_event.events & READ)) {
					// wakeup
					// DebugLog << "epoll wakeup, fd=[" << m_wake_fd << "]";
					char buf[8];
					while(1) {
						if((g_sys_read_fun(m_wake_fd, buf, 8) == -1) && errno == EAGAIN) {
							break;
						}
					}

				} else {
					tinyrpc::FdEvent* ptr = (tinyrpc::FdEvent*)one_event.data.ptr;
          if (ptr != nullptr) {
            int fd = ptr->getFd();

            if ((!(one_event.events & EPOLLIN)) && (!(one_event.events & EPOLLOUT))){
              ErrorLog << "socket [" << fd << "] occur other unknow event:[" << one_event.events << "], need unregister this socket";
              delEventInLoopThread(fd);
            } else {
              // if register coroutine, pengding coroutine to common coroutine_tasks
              if (ptr->getCoroutine()) {
                // the first one coroutine when epoll_wait back, just directly resume by this thread, not add to global CoroutineTaskQueue
                // because every operate CoroutineTaskQueue should add mutex lock
                if (!first_coroutine) {
                  first_coroutine = ptr->getCoroutine();
                  continue;
                }
                if (m_reactor_type == SubReactor) {
                  delEventInLoopThread(fd);
                  ptr->setReactor(NULL);
                  CoroutineTaskQueue::GetCoroutineTaskQueue()->push(ptr);
                } else {
                  // main reactor, just resume this coroutine. it is accept coroutine. and Main Reactor only have this coroutine
                  tinyrpc::Coroutine::Resume(ptr->getCoroutine());
                  if (first_coroutine) {
                    first_coroutine = NULL;
                  }
                }

              } else {
                std::function<void()> read_cb;
                std::function<void()> write_cb;
                read_cb = ptr->getCallBack(READ);
                write_cb = ptr->getCallBack(WRITE);
                // if timer event, direct excute
                if (fd == m_timer_fd) {
                  read_cb();
                  continue;
                }
                if (one_event.events & EPOLLIN) {
                  // DebugLog << "socket [" << fd << "] occur read event";
                  Mutex::Lock lock(m_mutex);
                  m_pending_tasks.push_back(read_cb);						
                }
                if (one_event.events & EPOLLOUT) {
                  // DebugLog << "socket [" << fd << "] occur write event";
                  Mutex::Lock lock(m_mutex);
                  m_pending_tasks.push_back(write_cb);						
                }

              }

            }
          }

				}
				
			}

			std::map<int, epoll_event> tmp_add;
			std::vector<int> tmp_del;

			{
        Mutex::Lock lock(m_mutex);
				tmp_add.swap(m_pending_add_fds);
				m_pending_add_fds.clear();

				tmp_del.swap(m_pending_del_fds);
				m_pending_del_fds.clear();

			}
			for (auto i = tmp_add.begin(); i != tmp_add.end(); ++i) {
				// DebugLog << "fd[" << (*i).first <<"] need to add";
				addEventInLoopThread((*i).first, (*i).second);	
			}
			for (auto i = tmp_del.begin(); i != tmp_del.end(); ++i) {
				// DebugLog << "fd[" << (*i) <<"] need to del";
				delEventInLoopThread((*i));	
			}
		}
	}
  DebugLog << "reactor loop end";
  m_is_looping = false;
}
```



![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744704477565-9cfbfd25-ade5-4ab1-ac0f-11dc28d7c656.png)





#### 2.2.4 SubReactor执行流程
![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744705062706-a14560ac-4fe5-4219-9a66-d008970212c7.png)

+ read_hook注册到epoll，yile，epoll返回resume进行读取数据

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744705112703-41cd7e2d-7de0-4b59-92d7-efc6ca49293d.png)



![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744705330921-3da7a3fd-6b68-499d-8534-7c43bcf34f11.png)

+ 创建编码器进行解码事件
+ 根据事件进行事件分发

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744705412217-aee14591-e30c-453c-a5ed-ae8cc6af5ba3.png)

+ 解码

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744705553041-2302acd9-29a2-44e8-bbf2-de84b18b8f69.png)

+ 根据server进行处理Handle
+ 进行编码

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744705587698-dbced3d3-3dff-41ad-8a23-cf7e4d110f0a.png)







## 三、其他
![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744970429576-de643c45-e3c4-4e84-b7b5-6e8af3280602.png)

### 3.1 阻塞和非阻塞
+ 阻塞

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744706666315-270b924f-4ce2-47c6-b532-656b30d66206.png)

```plain
void TinyPbRpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method, 
    google::protobuf::RpcController* controller, 
    const google::protobuf::Message* request, 
    google::protobuf::Message* response, 
    google::protobuf::Closure* done) {

// 生成controller,设置地址
  TinyPbStruct pb_struct;
  TinyPbRpcController* rpc_controller = dynamic_cast<TinyPbRpcController*>(controller);
  if (!rpc_controller) {
    ErrorLog << "call failed. falid to dynamic cast TinyPbRpcController";
    return;
  }

// 生成tcpclient
  TcpClient::ptr m_client = std::make_shared<TcpClient>(m_addr);
  rpc_controller->SetLocalAddr(m_client->getLocalAddr());
  rpc_controller->SetPeerAddr(m_client->getPeerAddr());
// 填充TinyPbStruct pd_struct包
  pb_struct.service_full_name = method->full_name();
  DebugLog << "call service_name = " << pb_struct.service_full_name;
  if (!request->SerializeToString(&(pb_struct.pb_data))) {
    ErrorLog << "serialize send package error";
    return;
  }

  if (!rpc_controller->MsgSeq().empty()) {
    pb_struct.msg_req = rpc_controller->MsgSeq();
  } else {
    // get current coroutine's msgno to set this request
    RunTime* run_time = getCurrentRunTime();
    if(run_time != NULL && !run_time->m_msg_no.empty()) {
      pb_struct.msg_req = run_time->m_msg_no;
      DebugLog << "get from RunTime succ, msgno = " << pb_struct.msg_req;
    } else {
      pb_struct.msg_req = MsgReqUtil::genMsgNumber();
      DebugLog << "get from RunTime error, generate new msgno = " << pb_struct.msg_req;
    }
    rpc_controller->SetMsgReq(pb_struct.msg_req);
  }

  // 编码
  AbstractCodeC::ptr m_codec = m_client->getConnection()->getCodec();
  m_codec->encode(m_client->getConnection()->getOutBuffer(), &pb_struct);
  if (!pb_struct.encode_succ) {
    rpc_controller->SetError(ERROR_FAILED_ENCODE, "encode tinypb data error");
    return;
  }

  InfoLog << "============================================================";
  InfoLog << pb_struct.msg_req << "|" << rpc_controller->PeerAddr()->toString() 
      << "|. Set client send request data:" << request->ShortDebugString();
  InfoLog << "============================================================";
  m_client->setTimeout(rpc_controller->Timeout());

  // 发送请求，或得res_data
  TinyPbStruct::pb_ptr res_data;
  int rt = m_client->sendAndRecvTinyPb(pb_struct.msg_req, res_data);
  if (rt != 0) {
    rpc_controller->SetError(rt, m_client->getErrInfo());
    ErrorLog << pb_struct.msg_req << "|call rpc occur client error, service_full_name=" << pb_struct.service_full_name << ", error_code=" 
        << rt << ", error_info = " << m_client->getErrInfo();
    return;
  }

  // 解码返回数据
  if (!response->ParseFromString(res_data->pb_data)) {
    rpc_controller->SetError(ERROR_FAILED_DESERIALIZE, "failed to deserialize data from server");
    ErrorLog << pb_struct.msg_req << "|failed to deserialize data";
    return;
  }
  if (res_data->err_code != 0) {
    ErrorLog << pb_struct.msg_req << "|server reply error_code=" << res_data->err_code << ", err_info=" << res_data->err_info;
    rpc_controller->SetError(res_data->err_code, res_data->err_info);
    return;
  }

  InfoLog<< "============================================================";
  InfoLog<< pb_struct.msg_req << "|" << rpc_controller->PeerAddr()->toString()
      << "|call rpc server [" << pb_struct.service_full_name << "] succ" 
      << ". Get server reply response data:" << response->ShortDebugString();
  InfoLog<< "============================================================";

  // excute callback function
  if (done) {
    done->Run();
  }
}
```



![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744967546827-f1bc38cb-8a34-4a35-9e98-f6f986134742.png)



+ 非阻塞

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744771856459-ffe6e884-f277-42ae-b134-46d5ac488fdd.png)



![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744783948285-0966df7e-f4c4-485e-833c-bb1c9c1e5e27.png)

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744972201830-8438569e-ca4b-4506-9a6b-9fa1c76af6b5.png)

### 3.2 时间轮
注册到epoll中等待被唤醒

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744708309830-e4b9457a-7e5e-4895-8b0c-70600ad987d2.png)

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744708346154-5f3bbeee-fb94-4543-8098-93e9dd1367b9.png)

新来的客户端连接火创建一个tcpconnection链接

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744708152991-1c42e45d-39e8-462e-8412-1927ac881c57.png)

当收到数据后也会创建一个链接

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744708240686-5a47079f-1364-4471-a4ae-c874bcacac4e.png)

![](https://cdn.nlark.com/yuque/0/2025/png/34771315/1744708231424-d3a6fc14-7459-466e-9788-0292265cc6da.png)

