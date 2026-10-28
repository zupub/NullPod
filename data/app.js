        let currentPath = '/';
        let uploadQueue = [];
        let currentUploadIndex = -1;
        let uploadStartTime = 0;
        
        // 页面加载完成后初始化
        document.addEventListener('DOMContentLoaded', function() {
            loadFileList(currentPath);
            loadStorageInfo();
            
            // 设置上传区域点击事件
            document.getElementById('uploadArea').addEventListener('click', function() {
                document.getElementById('fileInput').click();
            });
            
            // 设置文件选择变化事件
            document.getElementById('fileInput').addEventListener('change', function(e) {
                if (e.target.files.length > 0) {
                    addFilesToQueue(Array.from(e.target.files));
                    document.getElementById('uploadBtn').disabled = false;
                }
            });
            
            // 设置拖放事件
            const uploadArea = document.getElementById('uploadArea');
            uploadArea.addEventListener('dragover', function(e) {
                e.preventDefault();
                uploadArea.classList.add('dragover');
            });
            
            uploadArea.addEventListener('dragleave', function() {
                uploadArea.classList.remove('dragover');
            });
            
            uploadArea.addEventListener('drop', function(e) {
                e.preventDefault();
                uploadArea.classList.remove('dragover');
                const files = e.dataTransfer.files;
                
                if (files.length > 0) {
                    addFilesToQueue(Array.from(files));
                    document.getElementById('uploadBtn').disabled = false;
                }
            });
            
            // 设置上传按钮点击事件
            document.getElementById('uploadBtn').addEventListener('click', function() {
                startUpload();
            });
        });
        
        // 添加文件到上传队列
        function addFilesToQueue(files) {
            files.forEach(file => {
                uploadQueue.push({
                    file: file,
                    status: 'queued',
                    progress: 0
                });
            });
            
            updateUploadQueueDisplay();
        }
        
        // 更新上传队列显示
        function updateUploadQueueDisplay() {
            const queueContainer = document.getElementById('uploadQueue');
            if (!queueContainer) return;
            
            queueContainer.innerHTML = '';
            
            uploadQueue.forEach((item, index) => {
                const queueItem = document.createElement('div');
                queueItem.className = 'queue-item';
                
                queueItem.innerHTML = `
                    <div class="queue-item-name">${item.file.name}</div>
                    <div class="queue-item-size">${formatFileSize(item.file.size)}</div>
                    <div class="queue-item-status status-${item.status}">${getStatusText(item.status)}</div>
                `;
                
                queueContainer.appendChild(queueItem);
            });
        }
        
        // 获取状态文本
        function getStatusText(status) {
            switch(status) {
                case 'queued': return '等待中';
                case 'uploading': return '上传中';
                case 'completed': return '已完成';
                case 'error': return '错误';
                default: return status;
            }
        }
        
        // 开始上传
        function startUpload() {
            if (uploadQueue.length === 0) return;
            
            document.getElementById('uploadBtn').disabled = true;
            document.getElementById('progressContainer').style.display = 'block';
            currentUploadIndex = 0;
            
            uploadNextFile();
        }
        
        // 上传下一个文件
        function uploadNextFile() {
            if (currentUploadIndex >= uploadQueue.length) {
                // 所有文件上传完成
                document.getElementById('uploadBtn').disabled = false;
                document.getElementById('progressContainer').style.display = 'none';
                showStatus('所有文件上传完成', false);
                loadFileList(currentPath);
                loadStorageInfo();
                uploadQueue = [];
                updateUploadQueueDisplay();
                return;
            }
            
            const queueItem = uploadQueue[currentUploadIndex];
            queueItem.status = 'uploading';
            updateUploadQueueDisplay();
            
            uploadFile(queueItem.file, currentUploadIndex);
        }
        
        // 上传单个文件
        function uploadFile(file, index) {
            const formData = new FormData();
            formData.append('data', file);
            formData.append('path', currentPath);
            
            const xhr = new XMLHttpRequest();
            uploadStartTime = Date.now();
            
            // 上传进度事件
            xhr.upload.addEventListener('progress', function(e) {
                if (e.lengthComputable) {
                    const percentComplete = (e.loaded / e.total) * 100;
                    updateProgress(percentComplete, e.loaded, e.total);
                    
                    // 更新队列项进度
                    uploadQueue[index].progress = percentComplete;
                    uploadQueue[index].status = 'uploading';
                    updateUploadQueueDisplay();
                }
            });
            
            // 上传完成事件
            xhr.addEventListener('load', function() {
                if (xhr.status === 200) {
                    uploadQueue[index].status = 'completed';
                    uploadQueue[index].progress = 100;
                    showStatus(`文件 "${file.name}" 上传成功`, false);
                    
                    currentUploadIndex++;
                    uploadNextFile();
                } else {
                    uploadQueue[index].status = 'error';
                    showStatus(`文件 "${file.name}" 上传失败: ` + xhr.responseText, true);
                    
                    currentUploadIndex++;
                    uploadNextFile();
                }
            });
            
            // 上传错误事件
            xhr.addEventListener('error', function() {
                uploadQueue[index].status = 'error';
                showStatus(`文件 "${file.name}" 上传失败`, true);
                
                currentUploadIndex++;
                uploadNextFile();
            });
            
            xhr.open('POST', '/upload');
            xhr.send(formData);
        }
        
        // 更新进度显示
        function updateProgress(percent, loaded, total) {
            const progressBar = document.getElementById('progress');
            const progressText = document.getElementById('progressText');
            const speedText = document.getElementById('speedText');
            
            if (progressBar) progressBar.style.width = percent + '%';
            if (progressText) progressText.textContent = `上传中: ${percent.toFixed(1)}%`;
            
            // 计算上传速度
            const currentTime = Date.now();
            const elapsedTime = (currentTime - uploadStartTime) / 1000; // 转换为秒
            if (elapsedTime > 0 && speedText) {
                const speed = loaded / elapsedTime; // 字节/秒
                speedText.textContent = `${formatFileSize(speed)}/s`;
            }
        }
        
        // 加载文件列表
        function loadFileList(path) {
            currentPath = path;
            updateBreadcrumb(path);
            
            fetch('/list?path=' + encodeURIComponent(path))
                .then(response => response.json())
                .then(data => {
                    const fileListContent = document.getElementById('fileListContent');
                    fileListContent.innerHTML = '';
                    
                    if (data.files.length === 0) {
                        fileListContent.innerHTML = '<div class="file-item"><div>此文件夹为空</div></div>';
                        return;
                    }
                    
                    // 先显示文件夹
                    data.files.filter(item => item.isDirectory).forEach(item => {
                        fileListContent.appendChild(createFileItem(item));
                    });
                    
                    // 再显示文件
                    data.files.filter(item => !item.isDirectory).forEach(item => {
                        fileListContent.appendChild(createFileItem(item));
                    });
                })
                .catch(error => {
                    console.error('加载文件列表失败:', error);
                    showStatus('加载文件列表失败: ' + error.message, true);
                });
        }
        
        // 创建文件项HTML
        function createFileItem(item) {
            const fileItem = document.createElement('div');
            fileItem.className = 'file-item';
            
            const icon = item.isDirectory ? '📁' : '📄';
            const size = item.isDirectory ? '-' : formatFileSize(item.size);
            const date = formatDate(item.modified);
            
            fileItem.innerHTML = `
                <div class="${item.isDirectory ? 'folder-item' : ''}">
                    <span class="file-icon ${item.isDirectory ? 'folder-icon' : ''}">${icon}</span>
                    ${item.isDirectory ? 
                        `<a href="#" onclick="loadFileList('${item.path}'); return false;">${item.name}</a>` : 
                        item.name
                    }
                </div>
                <div class="file-size">${size}</div>
                <div>${date}</div>
                <div class="file-actions">
                    ${!item.isDirectory ? 
                        `<button class="action-btn" onclick="downloadFile('${item.path}')" title="下载">⬇️</button>` : 
                        ''
                    }
                    <button class="action-btn" onclick="deleteItem('${item.path}', ${item.isDirectory})" title="删除">🗑️</button>
                </div>
            `;
            
            return fileItem;
        }
        
        // 更新面包屑导航
        function updateBreadcrumb(path) {
            const breadcrumb = document.getElementById('breadcrumb');
            const parts = path.split('/').filter(part => part !== '');
            
            let breadcrumbHTML = '<a href="#" onclick="loadFileList(\'/\'); return false;">根目录</a>';
            let currentPath = '';
            
            parts.forEach(part => {
                currentPath += '/' + part;
                breadcrumbHTML += `<span class="breadcrumb-separator">/</span>
                                  <a href="#" onclick="loadFileList('${currentPath}'); return false;">${part}</a>`;
            });
            
            breadcrumb.innerHTML = breadcrumbHTML;
        }
        
        // 刷新文件列表
        function refreshFileList() {
            loadFileList(currentPath);
            loadStorageInfo();
        }
        
        // 返回上级目录
        function goUp() {
            if (currentPath === '/') return;
            
            const parts = currentPath.split('/').filter(part => part !== '');
            parts.pop(); // 移除最后一部分
            
            const newPath = parts.length > 0 ? '/' + parts.join('/') : '/';
            loadFileList(newPath);
        }
        
        // 创建文件夹
        function createFolder() {
            const folderNameInput = document.getElementById('newFolderName');
            const folderName = folderNameInput.value.trim();
            
            if (!folderName) {
                showStatus('请输入文件夹名称', true);
                return;
            }
            
            const fullPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + folderName;
            
            fetch('/mkdir?path=' + encodeURIComponent(fullPath), {
                method: 'POST'
            })
            .then(response => response.json())
            .then(result => {
                if (result.message) {
                    showStatus('文件夹创建成功', false);
                    folderNameInput.value = '';
                    loadFileList(currentPath);
                } else {
                    showStatus('文件夹创建失败: ' + result.error, true);
                }
            })
            .catch(error => {
                console.error('创建文件夹失败:', error);
                showStatus('文件夹创建失败: ' + error.message, true);
            });
        }
        
        // 下载文件
        function downloadFile(filePath) {
            window.open('/download?path=' + encodeURIComponent(filePath), '_blank');
        }
        
        // 删除文件或文件夹
        function deleteItem(path, isDirectory) {
            const itemType = isDirectory ? '文件夹' : '文件';
            const itemName = path.split('/').pop();
            
            if (!confirm(`确定要删除${itemType} "${itemName}" 吗？此操作不可撤销。`)) {
                return;
            }
            
            fetch('/delete?path=' + encodeURIComponent(path) + '&isDirectory=' + isDirectory, {
                method: 'DELETE'
            })
            .then(response => response.json())
            .then(result => {
                if (result.message) {
                    showStatus(`${itemType} "${itemName}" 删除成功`, false);
                } else {
                    showStatus(`${itemType} "${itemName}" 删除失败: ` + result.error, true);
                }
                loadFileList(currentPath);
                loadStorageInfo();
            })
            .catch(error => {
                console.error('删除失败:', error);
                showStatus(`${itemType} "${itemName}" 删除失败: ` + error.message, true);
            });
        }
        
        // 加载存储信息
        function loadStorageInfo() {
            fetch('/storage')
                .then(response => response.json())
                .then(data => {
                    const storageInfo = document.getElementById('storageInfo');
                    const usedPercent = (data.usedBytes / data.totalBytes * 100).toFixed(1);
                    
                    storageInfo.innerHTML = `
                        <p>总空间: ${formatFileSize(data.totalBytes)}</p>
                        <p>已使用: ${formatFileSize(data.usedBytes)} (${usedPercent}%)</p>
                        <p>可用空间: ${formatFileSize(data.totalBytes - data.usedBytes)}</p>
                    `;
                })
                .catch(error => {
                    console.error('加载存储信息失败:', error);
                });
        }
        
        // 显示状态消息
        function showStatus(message, isError) {
            // 创建一个状态消息元素
            const statusMessage = document.createElement('div');
            statusMessage.textContent = message;
            statusMessage.className = 'status-message ' + (isError ? 'error' : 'success');
            
            // 添加到页面顶部
            document.body.insertBefore(statusMessage, document.body.firstChild);
            
            // 5秒后自动移除
            setTimeout(() => {
                if (statusMessage.parentNode) {
                    statusMessage.parentNode.removeChild(statusMessage);
                }
            }, 5000);
        }
        
        // 格式化文件大小
        function formatFileSize(bytes) {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
        }
        
        // 格式化日期
        function formatDate(timestamp) {
            if (!timestamp) return '-';
            const date = new Date(timestamp * 1000);
            return date.toLocaleDateString() + ' ' + date.toLocaleTimeString().slice(0, 5);
        }
