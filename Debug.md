```cpp
bool http_conn::write()
 2{
 3    int temp=0;
 4    int bytes_have_send=0;
 5    int bytes_to_send=m_write_idx;
 6    if(bytes_to_send==0)
 7    {
 8        modfd(m_epollfd,m_sockfd,EPOLLIN);
 9        init();
10        return true;
11    }
12    while(1)
13    {
14        temp=writev(m_sockfd,m_iv,m_iv_count);
15        if(temp<=-1)
16        {
17            if(errno==EAGAIN)
18            {
19                modfd(m_epollfd,m_sockfd,EPOLLOUT);
20                return true;
21            }
22            unmap();
23            return false;
24        }
25        bytes_to_send-=temp;
26        bytes_have_send+=temp;
27        if(bytes_to_send<=bytes_have_send)
28        {
29            unmap();
30            if(m_linger)
31            {
32                init();
33                modfd(m_epollfd,m_sockfd,EPOLLIN);
34                return true;
35            }
36            else
37            {
38                modfd(m_epollfd,m_sockfd,EPOLLIN);
39                return false;
40            }
41        }
42    }
43}

```

```cpp
bool http_conn::write()
 2{
 3    int temp = 0;
 4
 5    int newadd = 0;
 6
 7    if (bytes_to_send == 0)
 8    {
 9        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
10        init();
11        return true;
12    }
13
14    while (1)
15    {
16        temp = writev(m_sockfd, m_iv, m_iv_count);
17
18        if (temp >= 0)
19        {
20            bytes_have_send += temp;
21            newadd = bytes_have_send - m_write_idx;
22        }
23        else
24        {
25            if (errno == EAGAIN)
26            {
27                if (bytes_have_send >= m_iv[0].iov_len)
28                {
29                    m_iv[0].iov_len = 0;
30                    m_iv[1].iov_base = m_file_address + newadd;
31                    m_iv[1].iov_len = bytes_to_send;
32                }
33                else
34                {
35                    m_iv[0].iov_base = m_write_buf + bytes_have_send;
36                    m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
37                }
38                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
39                return true;
40            }
41            unmap();
42            return false;
43        }
44        bytes_to_send -= temp;
45        if (bytes_to_send <= 0)
46
47        {
48            unmap();
49            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
50
51            if (m_linger)
52            {
53                init();
54                return true;
55            }
56            else
57            {
58                return false;
59            }
60        }
61    }
62}




```